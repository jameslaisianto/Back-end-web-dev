/*
 Authorization Server code for CMPT 276, Spring 2016.
 */

#include <iostream>
#include <string>
#include <unordered_map>
#include <vector>
#include <tuple>

#include <cpprest/http_listener.h>
#include <cpprest/json.h>

#include <was/common.h>
#include <was/table.h>

#include "TableCache.h"
#include "make_unique.h"
#include "ClientUtils.h"
#include "ServerUtils.h"


//#include "azure_keys.h"
/*
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
 */
using std::cin;
using std::cout;
using std::endl;
using std::getline;
using std::make_pair;
using std::pair;
using std::string;
using std::unordered_map;
using std::vector;
using std::tuple;
using std::get;
using std::make_tuple;

using web::http::http_headers;
using web::http::http_request;
using web::http::methods;
using web::http::status_code;
using web::http::status_codes;
using web::http::uri;

using web::json::value;

using web::http::experimental::listener::http_listener;

using prop_str_vals_t = vector<pair<string,string>>;


constexpr const char* def_url = "http://localhost:34572";

const string auth_table_name {"AuthTable"};
const string auth_table_userid_partition {"Userid"};
const string auth_table_password_prop {"Password"};
const string auth_table_partition_prop {"DataPartition"};
const string auth_table_row_prop {"DataRow"};
const string data_table_name {"DataTable"};

const string get_read_token_op {"GetReadToken"};
const string get_update_token_op {"GetUpdateToken"};
const string get_update_data_op {"GetUpdateData"};
const string create_table_op {"CreateTableAdmin"};
const string delete_table_op {"DeleteTableAdmin"};

const string read_entity_admin {"ReadEntityAdmin"};
const string update_entity_admin {"UpdateEntityAdmin"};
const string delete_entity_admin {"DeleteEntityAdmin"};

const string read_entity_auth {"ReadEntityAuth"};
const string update_entity_auth {"UpdateEntityAuth"};
const string push_status {"PushStatus"};


//Address Declarations
static constexpr const char* addr {"http://localhost:34568/"};
static constexpr const char* auth_addr {"http://localhost:34570/"};
static constexpr const char* push_addr {"http://localhost:34574/"};

//End Address Declarations


//Cache of opened tables

//TableCache table_cache {};

/*
 Convert properties represented in Azure Storage type
 to prop_str_vals_t type.
 
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
 */
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
/*
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
 
 */
//Initialize Unordered_Map ===================

unordered_map<string, tuple<string,string,string>> SignedOn;
unordered_map<string, tuple<string,string,string>>::iterator it;



//End initialize Unordered_Map ================

/*
 Top-level routine for processing all HTTP GET requests.
 */
void handle_get(http_request message) {
    
    string path {uri::decode(message.relative_uri().path())};
    cout << endl << "**** AuthServer GET " << path << endl;
    auto paths = uri::split_path(path);
    unordered_map<string,string> json_body {get_json_body(message)};
    
    //User Data from tuple
    tuple<string,string,string> user_data = SignedOn[paths[1]];
    
    
    // Need at least an operation and userid
    if (paths.size() < 2) {
        message.reply(status_codes::BadRequest);
        return;
    }
    //json body cannot have more than 1 property
    if(json_body.size()>1){
        message.reply(status_codes::BadRequest);
        return;
    }
    //first string has to be Password and the password cannot be empty
    if(json_body.size()==1){
        for(const auto v:json_body){
            if(v.first!="Password"){
                message.reply(status_codes::BadRequest);
                return;
            }
            if(v.second.empty()){
                message.reply(status_codes::BadRequest);
                return;
            }
        }
    }
    
    //    cloud_table table {table_cache.lookup_table("AuthTable")};
    //    cloud_table data_table {table_cache.lookup_table("DataTable")};
    
    
    // // //UserID does not exist/wrong userID gives error 404
    // // table_operation retrieve_operation {table_operation::retrieve_entity(paths[1])};
    // // table_result retrieve_result{table.execute(retrieve_operation)};
    // //   if(retrieve_result.http_status_code()==status_codes::NotFound){
    // //     message.reply(status_codes::NotFound);
    // //     return;
    // //   }
    
    bool signed_in = false;             //set signed_in status for user as false
    for (SignedOn.begin(); it!=SignedOn.end(); it++) {
        if (it->first == paths[1]) {    //if user is found in the map, set as true
            signed_in = true;
        }
    }
    
    
    if (paths[0] == "ReadFriendList") {
        if (!signed_in) {
            message.reply(status_codes::Forbidden);
            return;
        }
        else{
            auto user_entity = do_request(methods::GET, addr + read_entity_auth + "/"+"DataTable"+"/"+ get<0>(user_data) + "/" + get<1>(user_data) + "/" + get<2>(user_data));
            auto entity_map = unpack_json_object(user_entity.second);
            string friends_list = entity_map["Friends"];
            value FriendList = build_json_value("Friends",friends_list);
            message.reply(status_codes::OK,FriendList);
            return;
            
        }
        
    }
    
    
    
}

/*
 Top-level routine for processing all HTTP POST requests.
 */
void handle_post(http_request message) {
    string path {uri::decode(message.relative_uri().path())};
    cout << endl << "**** POST " << path << endl;
    auto paths = uri::split_path(path);
    
    //User Data from tuple
    tuple<string,string,string> user_data = SignedOn[paths[1]];
    
    string userid = paths[1];
    unordered_map<string,string> json_body {get_json_body(message)};
    string pass {};
    string prop {};
    
    
    for(const auto v:json_body){
        if(v.first=="Password"){
            
            pass = v.second;
            prop = v.first;
        }
        else{
            message.reply(status_codes::BadRequest);
            cout << "There is no password" << endl; //Debug
            return;
        }
    }
    cout << prop << ": " << pass << endl;   //Debug
    
    
    if (paths[0] == "SignOn") {
        cout << "Entering SignOn" << endl;  //Debug
        pair<string,string> pswd = make_pair(prop,pass);
        cout << "User ID is: " << paths[1] << pswd.first << ": " << pswd.second << endl;  //Debug
        value password = build_json_value(pswd);
        auto status = do_request(methods::GET, auth_addr + get_update_token_op + "/" + paths[1],password);
        cout << "Status Code: " << status.first << endl;    //Debug
        if (status.first == status_codes::OK) {
            auto update_data = unpack_json_object(status.second);
            pair<string,tuple<string,string,string>> client = make_pair(userid,make_tuple(update_data["token"],update_data["DataPartition"],update_data["DataRow"]));
            SignedOn.insert(client); //Insert the userid & token into SignedOn status
            message.reply(status_codes::OK,status.second);
            return;
        }
    }
    
    if (paths[0] == "SignOff") {
        cout << "Entering SignOff" << endl; //Debug
        for (SignedOn.begin(); it!=SignedOn.end(); it++){
            if (it->first == paths[1]) {
                SignedOn.erase(paths[1]);   //Erase the userid and token from SignedOn status
                message.reply(status_codes::OK);
                return;
            }
        }
        message.reply(status_codes::NotFound);
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
    
    //User Data from tuple
    tuple<string,string,string> user_data = SignedOn[paths[1]];
    
    bool signed_in = false;             //set signed_in status for user as false
    for (SignedOn.begin(); it!=SignedOn.end(); it++) {
        if (it->first == paths[1]) {    //if user is found in the map, set as true
            signed_in = true;
        }
    }
    
    //GET THE FRIEND LIST
    auto user_entity = do_request(methods::GET, addr + read_entity_auth + "/"+"DataTable"+"/"+ get<0>(user_data) + "/" + paths[1] + "/" + paths[2]);
    auto entity_map = unpack_json_object(user_entity.second);
    string friends_list = entity_map["Friends"];
    friends_list_t friends_list_parsed = parse_friends_list(friends_list); //parse the friends list from previous line, returns vector of pairs
    ////////////
    
    
    if (paths[0] == "AddFriend") {  //method for adding a friend
        if (!signed_in) {
            message.reply(status_codes::Forbidden);
            return;
        }
        else{
            //check if friend already exists in the list, if it is return status code ok
            for (auto const& v: friends_list_parsed) {
                if (v.first == paths[1] && v.second == paths[2]) {
                    message.reply(status_codes::OK);
                    cout << "The person exists in the table" << endl;
                    return;
                }
            }
            //Add the friend into friend list if he/she does not exist in the list
            pair<string,string> friend_to_be_added = make_pair(paths[1],paths[2]);
            friends_list_parsed.push_back(friend_to_be_added);
            string new_string_of_friends = friends_list_to_string (friends_list_parsed); //friend list string
            auto new_friend_list = do_request(methods::GET, addr + update_entity_auth + "/"+"DataTable"+"/"+ get<0>(user_data) + "/" + paths[1] + "/" + paths[2],build_json_value("Friends",new_string_of_friends));
            if (new_friend_list.first == status_codes::OK) {
                message.reply(status_codes::OK);
                return;
            }
            
        }
        
        
    }
    
    
    
    if (paths[0] == "UnFriend") {  //method for deleting a friend
        if (!signed_in) {
            message.reply(status_codes::Forbidden);
            return;
        }
        else{
            //check if friend already exists in the list, if he/she is remove the friend
            for (auto v = friends_list_parsed.begin(); v != friends_list_parsed.end(); v++) {
                if (v->first == paths[2] && v->second == paths[3]) {
                    string friend_to_be_deleted = paths[1];
                    friends_list_parsed.erase(v); //removing the friend
                    string new_string_of_friends = friends_list_to_string(friends_list_parsed);
                    auto new_friend_list = do_request(methods::GET, addr + update_entity_auth + "/"+"DataTable"+"/"+ get<0>(user_data) + "/" + paths[1] + "/" + paths[2],build_json_value("Friends",new_string_of_friends));
                    if (new_friend_list.first == status_codes::OK) {
                        message.reply(status_codes::OK);
                        return;
                    }
                }
                
            }
            //Friend does not exist in the friend list, return OK
            cout << "The person does not exist in your list" << endl;
            message.reply(status_codes::OK);
            return;
        }
        
    }
    
    
    if (paths[0] == "UpdateStatus") {  //method for updating status
        if (!signed_in) {
            message.reply(status_codes::Forbidden);
            return;
        }
        else{
            string string_of_friends = friends_list_to_string(friends_list_parsed);
            pair<status_code, value> result = do_request(methods::POST, push_addr + push_status +"/"+get<1>(user_data)+"/"+get<2>(user_data)+"/"+paths[2]);
            
        }
    }
    
    
}

/*
 Top-level routine for processing all HTTP DELETE requests.
 */
void handle_delete(http_request message) {
    string path {uri::decode(message.relative_uri().path())};
    cout << endl << "**** DELETE " << path << endl;
}

/*
 Main authentication server routine
 
 Install handlers for the HTTP requests and open the listener,
 which processes each request asynchronously.
 
 Note that, unlike BasicServer, AuthServer only
 installs the listeners for GET. Any other HTTP
 method will produce a Method Not Allowed (405)
 response.
 
 If you want to support other methods, uncomment
 the call below that hooks in a the appropriate
 listener.
 
 Wait for a carriage return, then shut the server down.
 */
int main (int argc, char const * argv[]) {
    cout << "AuthServer: Parsing connection string" << endl;
    //table_cache.init (storage_connection_string);
    
    cout << "AuthServer: Opening listener" << endl;
    http_listener listener {def_url};
    listener.support(methods::GET, &handle_get);
    listener.support(methods::POST, &handle_post);
    listener.support(methods::PUT, &handle_put);
    listener.support(methods::DEL, &handle_delete);
    listener.open().wait(); // Wait for listener to complete starting
    
    cout << "Enter carriage return to stop AuthServer." << endl;
    string line;
    getline(std::cin, line);
    
    // Shut it down
    listener.close().wait();
    cout << "AuthServer closed" << endl;
}
