/*
 Authorization Server code for CMPT 276, Spring 2016.
 */

#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>

#include <cpprest/http_listener.h>
#include <cpprest/json.h>

#include <was/common.h>
#include <was/table.h>

#include "TableCache.h"
#include "make_unique.h"



using azure::storage::storage_exception;
using azure::storage::cloud_table;
using azure::storage::cloud_table_client;
using azure::storage::edm_type;
using azure::storage::entity_property;
using azure::storage::table_entity;
using azure::storage::table_operation;
using azure::storage::table_request_options;
using azure::storage::table_result;
using azure::storage::table_shared_access_policy;
using azure::storage::table_query;
using azure::storage::table_query_iterator;
using azure::storage::table_result;

using std::cin;
using std::cout;
using std::endl;
using std::getline;
using std::make_pair;
using std::pair;
using std::string;
using std::unordered_map;
using std::vector;

using web::http::http_headers;
using web::http::http_request;
using web::http::methods;
using web::http::status_code;
using web::http::status_codes;
using web::http::uri;

using web::json::value;

using web::http::experimental::listener::http_listener;

using prop_str_vals_t = vector<pair<string,string>>;

constexpr const char* def_url = "http://localhost:34574/";
constexpr const char* data_table_addr = "http://localhost:34568/";


const string auth_table_name {"AuthTable"};
const string auth_table_userid_partition {"Userid"};
const string auth_table_password_prop {"Password"};
const string auth_table_partition_prop {"DataPartition"};
const string auth_table_row_prop {"DataRow"};
const string data_table_name {"DataTable"};

const string read_entity_admin {"ReadEntityAdmin"};
const string update_entity_admin {"UpdateEntityAdmin"};
const string delete_entity_admin {"DeleteEntityAdmin"};

const string get_read_token_op {"GetReadToken"};
const string get_update_token_op {"GetUpdateToken"};

const string post_push_status_op {"PushStatus"};

const string update_prop {"Updates"};


pair<status_code,value> do_request (const method& http_method, const string& uri_string, const value& req_body) {
    http_request request {http_method};
    if (req_body != value {}) {
        http_headers& headers (request.headers());
        headers.add("Content-Type", "application/json");
        request.set_body(req_body);
    }
    
    status_code code;
    value resp_body;
    http_client client {uri_string};
    client.request (request)
    .then([&code](http_response response)
          {
              code = response.status_code();
              const http_headers& headers {response.headers()};
              auto content_type (headers.find("Content-Type"));
              if (content_type == headers.end() ||
                  content_type->second != "application/json")
                  return pplx::task<value> ([] { return value {};});
              else
                  return response.extract_json();
          })
    .then([&resp_body](value v) -> void
          {
              resp_body = v;
              return;
          })
    .wait();
    return make_pair(code, resp_body);
}

// Version that defaults third argument
pair<status_code,value> do_request (const method& http_method, const string& uri_string) {
    return do_request (http_method, uri_string, value {});
}

/*
 Convert properties represented in Azure Storage type
 to prop_str_vals_t type.
 */
prop_str_vals_t get_string_properties (const table_entity::properties_type& properties) {
    prop_str_vals_t values {};
    for (const auto v : properties) {
        if (v.second.property_type() == edm_type::string) {
            values.push_back(make_pair(v.first,v.second.string_value()));
        }
        else {
            // Force the value as string in any case
            values.push_back(make_pair(v.first, v.second.str()));
        }
    }
    return values;
}

value build_json_object (const vector<pair<string,string>>& properties) {
    value result {value::object ()};
    for (auto& prop : properties) {
        result[prop.first] = value::string(prop.second);
    }
    return result;
}


/*
 Given an HTTP message with a JSON body, return the JSON
 body as an unordered map of strings to strings.
 
 Note that all types of JSON values are returned as strings.
 Use C++ conversion utilities to convert to numbers or dates
 as necessary.
 */
unordered_map<string,string> get_json_body(http_request message) {
    unordered_map<string,string> results {};
    const http_headers& headers {message.headers()};
    auto content_type (headers.find("Content-Type"));
    if (content_type == headers.end() ||
        content_type->second != "application/json")
        return results;
    
    value json{};
    message.extract_json(true)
    .then([&json](value v) -> bool
          {
              json = v;
              return true;
          })
    .wait();
    
    if (json.is_object()) {
        for (const auto& v : json.as_object()) {
            if (v.second.is_string()) {
                results[v.first] = v.second.as_string();
            }
            else {
                results[v.first] = v.second.serialize();
            }
        }
    }
    return results;
}

/*
 Return a token for 24 hours of access to the specified table,
 for the single entity defind by the partition and row.
 
 permissions: A bitwise OR ('|')  of table_shared_access_poligy::permission
 constants.
 
 For read-only:
 table_shared_access_policy::permissions::read
 For read and update:
 table_shared_access_policy::permissions::read |
 table_shared_access_policy::permissions::update
 */
pair<status_code,string> do_get_token (const cloud_table& data_table,
                                       const string& partition,
                                       const string& row,
                                       uint8_t permissions) {
    
    utility::datetime exptime {utility::datetime::utc_now() + utility::datetime::from_days(1)};
    try {
        string limited_access_token {
            data_table.get_shared_access_signature(table_shared_access_policy {
                exptime,
                permissions},
                                                   string(), // Unnamed policy
                                                   // Start of range (inclusive)
                                                   partition,
                                                   row,
                                                   // End of range (inclusive)
                                                   partition,
                                                   row)
            // Following token allows read access to entire table
            //table.get_shared_access_signature(table_shared_access_policy {exptime, permissions})
        };
        cout << "Token " << limited_access_token << endl;
        return make_pair(status_codes::OK, limited_access_token);
    }
    catch (const storage_exception& e) {
        cout << "Azure Table Storage error: " << e.what() << endl;
        cout << e.result().extended_error().message() << endl;
        return make_pair(status_codes::InternalError, string{});
    }
}

/*
 Top-level routine for processing all HTTP GET requests.
 */
void handle_get(http_request message) {
    string path {uri::decode(message.relative_uri().path())};
    cout << endl << "**** AuthServer GET " << path << endl;
    auto paths = uri::split_path(path);
}

/*
 Top-level routine for processing all HTTP POST requests.
 */
void handle_post(http_request message) {
  string path {uri::decode(message.relative_uri().path())};
  cout << endl << "**** PushServer POST " << path << endl;
  auto paths = uri::split_path(path);
  unordered_map<string,string> json_body {get_json_body(message)};

  if(paths[0]==post_push_status_op && json_body.size()>0){
    string user_friend {json_body["Friends"]};
    friends_list_t friend_vector {parse_friends_list(user_friend)};
    string friend_status {paths[3]};
      
      
    auto it = user_friend.begin();
    while (it != user_friend.end()){
      string country_name = it->first;
      string friend_name = it->second;
        
      pair<status_code,value> prop_of_friend {do_request(methods::GET,data_table_addr +
                                                            read_entity_admin +"/" +
                                                            country_name + "/" +
                                                            friend_name)}; 

      if(prop_of_friend.first == status_codes::OK){
        string updates_str {get_json_object_prop(prop_of_friend.second, update_prop)};
          
        //add the updates
        updates_str = updates_str + friend_status +"\n";

        do_request(methods::PUT, data_table_addr + update_entity_admin + "/" + data_table_name +
                    "/"+ country_name + "/" + friend_name, value::object(vector<pair<string,value>> 
                                                              {make_pair(update_prop, value::string(updates_str))}));
      }
      it++;
    }
    message.reply(status_codes::OK);
    return;  
  } 
}


/*
 Top-level routine for processing all HTTP PUT requests.
 */
void handle_put(http_request message) {
    string path {uri::decode(message.relative_uri().path())};
    cout << endl << "**** PUT " << path << endl;
    auto paths = uri::split_path(path);
}

/*
 Top-level routine for processing all HTTP DELETE requests.
 */
void handle_delete(http_request message) {
    string path {uri::decode(message.relative_uri().path())};
    cout << endl << "**** DELETE " << path << endl;
}

/*
 Main push server routine
 
 Install handlers for the HTTP requests and open the listener,
 which processes each request asynchronously.
 
 Wait for a carriage return, then shut the server down.
 */
int main (int argc, char const * argv[]) {
    cout << "PushServer: Parsing connection string" << endl;
    
    
    cout << "PushServer: Opening listener" << endl;
    http_listener listener {def_url};
    //listener.support(methods::GET, &handle_get);
    listener.support(methods::POST, &handle_post);
    //listener.support(methods::PUT, &handle_put);
    //listener.support(methods::DEL, &handle_delete);
    listener.open().wait(); // Wait for listener to complete starting
    
    cout << "Enter carriage return to stop PushServer." << endl;
    string line;
    getline(std::cin, line);
    
    // Shut it down
    listener.close().wait();
    cout << "PushServer closed" << endl;
}
