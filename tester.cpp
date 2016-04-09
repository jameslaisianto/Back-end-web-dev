/*
 Sample unit tests for BasicServer
 */

#include <algorithm>
#include <exception>
#include <iostream>
#include <string>
#include <utility>
#include <vector>

#include <cpprest/http_client.h>
#include <cpprest/json.h>

#include <pplx/pplxtasks.h>

#include <UnitTest++/UnitTest++.h>


using std::cerr;
using std::cout;
using std::endl;
using std::make_pair;
using std::pair;
using std::string;
using std::vector;

using web::http::http_headers;
using web::http::http_request;
using web::http::http_response;
using web::http::method;
using web::http::methods;
using web::http::status_code;
using web::http::status_codes;
using web::http::uri_builder;

using web::http::client::http_client;

using web::json::object;
using web::json::value;

const string create_table_op {"CreateTableAdmin"};
const string delete_table_op {"DeleteTableAdmin"};

const string read_entity_admin {"ReadEntityAdmin"};
const string update_entity_admin {"UpdateEntityAdmin"};
const string delete_entity_admin {"DeleteEntityAdmin"};

const string read_entity_auth {"ReadEntityAuth"};
const string update_entity_auth {"UpdateEntityAuth"};

const string get_read_token_op  {"GetReadToken"};
const string get_update_token_op {"GetUpdateToken"};

// The two optional operations from Assignment 1
const string add_property_admin {"AddPropertyAdmin"};
const string update_property_admin {"UpdatePropertyAdmin"};

/*
 Make an HTTP request, returning the status code and any JSON value in the body
 
 method: member of web::http::methods
 uri_string: uri of the request
 req_body: [optional] a json::value to be passed as the message body
 
 If the response has a body with Content-Type: application/json,
 the second part of the result is the json::value of the body.
 If the response does not have that Content-Type, the second part
 of the result is simply json::value {}.
 
 You're welcome to read this code but bear in mind: It's the single
 trickiest part of the sample code. You can just call it without
 attending to its internals, if you prefer.
 */

// Version with explicit third argument
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
 Utility to create a table
 
 addr: Prefix of the URI (protocol, address, and port)
 table: Table in which to insert the entity
 */
int create_table (const string& addr, const string& table) {
    pair<status_code,value> result {do_request (methods::POST, addr + create_table_op + "/" + table)};
    return result.first;
}

/*
 Utility to compare two JSON objects
 
 This is an internal routine---you probably want to call compare_json_values().
 */
bool compare_json_objects (const object& expected_o, const object& actual_o) {
    CHECK_EQUAL (expected_o.size (), actual_o.size());
    if (expected_o.size() != actual_o.size())
        return false;
    
    bool result {true};
    for (auto& exp_prop : expected_o) {
        object::const_iterator act_prop {actual_o.find (exp_prop.first)};
        CHECK (actual_o.end () != act_prop);
        if (actual_o.end () == act_prop)
            result = false;
        else {
            CHECK_EQUAL (exp_prop.second, act_prop->second);
            if (exp_prop.second != act_prop->second)
                result = false;
        }
    }
    return result;
}

/*
 Utility to compare two JSON objects represented as values
 
 expected: json::value that was expected---must be an object
 actual: json::value that was actually returned---must be an object
 */
bool compare_json_values (const value& expected, const value& actual) {
    assert (expected.is_object());
    assert (actual.is_object());
    
    object expected_o {expected.as_object()};
    object actual_o {actual.as_object()};
    return compare_json_objects (expected_o, actual_o);
}

/*
 Utility to compre expected JSON array with actual
 
 exp: vector of objects, sorted by Partition/Row property
 The routine will throw if exp is not sorted.
 actual: JSON array value of JSON objects
 The routine will throw if actual is not an array or if
 one or more values is not an object.
 
 Note the deliberate asymmetry of the how the two arguments are handled:
 
 exp is set up by the test, so we *require* it to be of the correct
 type (vector<object>) and to be sorted and throw if it is not.
 
 actual is returned by the database and may not be an array, may not
 be values, and may not be sorted by partition/row, so we have
 to check whether it has those characteristics and convert it
 to a type comparable to exp.
 */
bool compare_json_arrays(const vector<object>& exp, const value& actual) {
    /*
     Check that expected argument really is sorted and
     that every value has Partion and Row properties.
     This is a precondition of this routine, so we throw
     if it is not met.
     */
    auto comp = [] (const object& a, const object& b) -> bool {
        return a.at("Partition").as_string()  <  b.at("Partition").as_string()
        ||
        (a.at("Partition").as_string() == b.at("Partition").as_string() &&
         a.at("Row").as_string()       <  b.at("Row").as_string());
    };
    if ( ! std::is_sorted(exp.begin(),
                          exp.end(),
                          comp))
        throw std::exception();
    
    // Check that actual is an array
    CHECK(actual.is_array());
    if ( ! actual.is_array())
        return false;
    web::json::array act_arr {actual.as_array()};
    
    // Check that the two arrays have same size
    CHECK_EQUAL(exp.size(), act_arr.size());
    if (exp.size() != act_arr.size())
        return false;
    
    // Check that all values in actual are objects
    bool all_objs {std::all_of(act_arr.begin(),
                               act_arr.end(),
                               [] (const value& v) { return v.is_object(); })};
    CHECK(all_objs);
    if ( ! all_objs)
        return false;
    
    // Convert all values in actual to objects
    vector<object> act_o {};
    auto make_object = [] (const value& v) -> object {
        return v.as_object();
    };
    std::transform (act_arr.begin(), act_arr.end(), std::back_inserter(act_o), make_object);
    
    /*
     Ensure that the actual argument is sorted.
     Unlike exp, we cannot assume this argument is sorted,
     so we sort it.
     */
    std::sort(act_o.begin(), act_o.end(), comp);
    
    // Compare the sorted arrays
    bool eq {std::equal(exp.begin(), exp.end(), act_o.begin(), &compare_json_objects)};
    CHECK (eq);
    return eq;
}

/*
 Utility to create JSON object value from vector of properties
 */
value build_json_object (const vector<pair<string,string>>& properties) {
    value result {value::object ()};
    for (auto& prop : properties) {
        result[prop.first] = value::string(prop.second);
    }
    return result;
}

/*
 Utility to delete a table
 
 addr: Prefix of the URI (protocol, address, and port)
 table: Table in which to insert the entity
 */
int delete_table (const string& addr, const string& table) {
    // SIGH--Note that REST SDK uses "methods::DEL", not "methods::DELETE"
    pair<status_code,value> result {
        do_request (methods::DEL,
                    addr + delete_table_op + "/" + table)};
    return result.first;
}

/*
 Utility to put an entity with a single property
 
 addr: Prefix of the URI (protocol, address, and port)
 table: Table in which to insert the entity
 partition: Partition of the entity
 row: Row of the entity
 prop: Name of the property
 pstring: Value of the property, as a string
 */
/*
 int put_entity(const string& addr, const string& table, const string& partition, const string& row, const string& prop, const string& pstring) {
 
 pair<status_code,value> result {
 do_request (methods::PUT,
 addr + update_entity_admin + "/" + table + "/" + partition + "/" + row,
 value::object (vector<pair<string,value>>
 {make_pair(prop, value::string(pstring))}))};
 return result.first;
 }
 */
int put_entity(const string& addr, const string& table, const string& partition, const string& row, const string& prop, const string& pstring) {
    pair<status_code,value> result {
        do_request (methods::PUT,
                    addr + "UpdateEntityAdmin/" + "/" + table + "/" + partition + "/" + row,
                    value::object (vector<pair<string,value>>
                                   {make_pair(prop, value::string(pstring))}))};
    return result.first;
    
}


/*
 Utility to put an entity with multiple properties
 
 addr: Prefix of the URI (protocol, address, and port)
 table: Table in which to insert the entity
 partition: Partition of the entity
 row: Row of the entity
 props: vector of string/value pairs representing the properties
 */
int put_entity(const string& addr, const string& table, const string& partition, const string& row,
               const vector<pair<string,value>>& props) {
    pair<status_code,value> result {
        do_request (methods::PUT,
                    addr + "UpdateEntityAdmin/" + table + "/" + partition + "/" + row,
                    value::object (props))};
    return result.first;
}

int put_entity_multiple_props(const string& addr, const string& table, const string& partition, const string& row, const vector<pair<string,value>>& mulprop ){
    pair<status_code,value> result {
        do_request (methods::PUT,
                    addr + "UpdateEntityAdmin/" + table + "/" + partition + "/" + row,
                    value::object (mulprop))};
    return result.first;
}

int put_entity_token(const string& addr, const string& table, const string& userid, const vector<pair<string,value>>& password){
    pair<status_code,value> result {
        do_request (methods::GET,
                    addr + "GetReadToken/" + table + "/" + userid + "/" , value::object (password))};
    return result.first;
}
//need json object consist of password, , DataPartition, and DataRow
/*
 Utility to delete an entity
 
 addr: Prefix of the URI (protocol, address, and port)
 table: Table in which to insert the entity
 partition: Partition of the entity
 row: Row of the entity
 */
int delete_entity (const string& addr, const string& table, const string& partition, const string& row)  {
    // SIGH--Note that REST SDK uses "methods::DEL", not "methods::DELETE"
    pair<status_code,value> result {
        do_request (methods::DEL,
                    addr + delete_entity_admin + "/" + table + "/" + partition + "/" + row)};
    return result.first;
}

/*
 Utility to get a token good for updating a specific entry
 from a specific table for one day.
 */
pair<status_code,string> get_update_token(const string& addr,  const string& userid, const string& password) {
    value pwd {build_json_object (vector<pair<string,string>> {make_pair("Password", password)})};
    pair<status_code,value> result {do_request (methods::GET,
                                                addr +
                                                get_update_token_op + "/" +
                                                userid,
                                                pwd
                                                )};
    cerr << "token " << result.second << endl;
    if (result.first != status_codes::OK)
        return make_pair (result.first, "");
    else {
        string token {result.second["token"].as_string()};
        return make_pair (result.first, token);
    }
}


pair<status_code,string> get_read_token(const string& addr,  const string& userid, const string& password) {
    value pwd {build_json_object (vector<pair<string,string>> {make_pair("Password", password)})};
    pair<status_code,value> result {do_request (methods::GET,
                                                addr +
                                                get_read_token_op + "/" +
                                                userid,
                                                pwd
                                                )};
    cerr << "token " << result.second << endl;
    if (result.first != status_codes::OK)
        return make_pair (result.first, "");
    else {
        string token {result.second["token"].as_string()};
        return make_pair (result.first, token);
    }
}

/*
 A sample fixture that ensures TestTable exists, and
 at least has the entity Franklin,Aretha/USA
 with the property "Song": "RESPECT".
 
 The entity is deleted when the fixture shuts down
 but the table is left. See the comments in the code
 for the reason for this design.
 
 */
SUITE(GET) {
    class GetFixture {
    public:
        static constexpr const char* addr {"http://127.0.0.1:34568/"};
        static constexpr const char* table {"TestTable"};
        static constexpr const char* partition {"Franklin,Aretha"};
        static constexpr const char* row {"USA"};
        static constexpr const char* property {"Song"};
        static constexpr const char* prop_val {"RESPECT"};
        //static constexpr const
    public:
        GetFixture() {
            int make_result {create_table(addr, table)};
            cerr << "create result " << make_result << endl;
            if (make_result != status_codes::Created && make_result != status_codes::Accepted) {
                throw std::exception();
            }
            int put_result {put_entity (addr, table, partition, row, property, prop_val)};
            cerr << "put result " << put_result << endl;
            if (put_result != status_codes::OK) {
                throw std::exception();
            }
        }
    };
    
    class BasicFixture {
    public:
        static constexpr const char* addr {"http://localhost:34568/"};
        static constexpr const char* table {"TestTable"};
        static constexpr const char* partition {"USA"};
        static constexpr const char* row {"Franklin,Aretha"};
        static constexpr const char* property {"Song"};
        static constexpr const char* prop_val {"RESPECT"};
        
    public:
        BasicFixture() {
            int make_result {create_table(addr, table)};
            cerr << "create result " << make_result << endl;
            if (make_result != status_codes::Created && make_result != status_codes::Accepted) {
                throw std::exception();
                
            }
            int put_result {put_entity (addr, table, partition, row, property, prop_val)};
            cerr << "put result " << put_result << endl;
            if (put_result != status_codes::OK) {
                throw std::exception();
            }
        }
        
        ~BasicFixture() {
            int del_ent_result {delete_entity (addr, table, partition, row)};
            if (del_ent_result != status_codes::OK) {
                throw std::exception();
            }
            
            /*
             In traditional unit testing, we might delete the table after every test.
             
             However, in cloud NoSQL environments (Azure Tables, Amazon DynamoDB)
             creating and deleting tables are rate-limited operations. So we
             leave the table after each test but delete all its entities.
             */
            cout << "Skipping table delete" << endl;
            /*
             int del_result {delete_table(addr, table)};
             cerr << "delete result " << del_result << endl;
             if (del_result != status_codes::OK) {
             throw std::exception();
             }
             */
        }
    };
    
    SUITE(GET) {
        /*
         A test of GET all table entries
         
         Demonstrates use of new compare_json_arrays() function.
         */
        TEST_FIXTURE(BasicFixture, GetAll) {
            string partition {"Canada"};
            string row {"Katherines,The"};
            string property {"Home"};
            string prop_val {"Vancouver"};
            int put_result {put_entity (BasicFixture::addr, BasicFixture::table, partition, row, property, prop_val)};
            cerr << "put result " << put_result << endl;
            assert (put_result == status_codes::OK);
            
            pair<status_code,value> result {
                do_request (methods::GET,
                            string(BasicFixture::addr)
                            + read_entity_admin + "/"
                            + string(BasicFixture::table))};
            CHECK_EQUAL(status_codes::OK, result.first);
            value obj1 {
                value::object(vector<pair<string,value>> {
                    make_pair(string("Partition"), value::string(partition)),
                    make_pair(string("Row"), value::string(row)),
                    make_pair(property, value::string(prop_val))
                })
            };
            value obj2 {
                value::object(vector<pair<string,value>> {
                    make_pair(string("Partition"), value::string(BasicFixture::partition)),
                    make_pair(string("Row"), value::string(BasicFixture::row)),
                    make_pair(string(BasicFixture::property), value::string(BasicFixture::prop_val))
                })
            };
            vector<object> exp {
                obj1.as_object(),
                obj2.as_object()
            };
            compare_json_arrays(exp, result.second);
            CHECK_EQUAL(status_codes::OK, delete_entity (BasicFixture::addr, BasicFixture::table, partition, row));
        }
        
        
        // A test of GET all table entries with partition BMW
        // EXPECT STATUS CODE 200: OK
        TEST_FIXTURE(GetFixture, GetSpecific1) {
            string partition {"BMW"};
            string row {"328i"};
            string property {"Score"};
            string prop_val {"A+"};
            int put_result {put_entity (GetFixture::addr, GetFixture::table, partition, row, property, prop_val)};
            cerr << "put result " << put_result << endl;
            assert (put_result == status_codes::OK);
            
            pair<status_code,value> result {
                do_request (methods::GET,
                            string(GetFixture::addr) + read_entity_admin + "/"
                            + string(GetFixture::table)+ "/" + "BMW" + "/" +"*" )};
            //CHECK_EQUAL(result.first.is_array(),status_codes::OK);
            //CHECK_EQUAL(2, result.first.as_array().size());
            /*
             Checking the body is not well-supported by UnitTest++, as we have to test
             independent of the order of returned values.
             */
            //CHECK_EQUAL(body.serialize(), string("{\"")+string(GetFixture::property)+ "\"unsure emoticon""+string(GetFixture::prop_val)+"\"}");
            CHECK_EQUAL(status_codes::OK, result.first);
            CHECK_EQUAL(status_codes::OK, delete_entity (GetFixture::addr, GetFixture::table, partition, row));
        }
        
        // A test of GET all table entries with partition Audi
        // EXPECT STATUS CODE 200: OK
        TEST_FIXTURE(GetFixture, GetSpecific2) {
            string partition {"Audi"};
            string row {"A4"};
            string property {"Horsepower"};
            string prop_val {"252"};
            int put_result {put_entity (GetFixture::addr, GetFixture::table, partition, row, property, prop_val)};
            cerr << "put result " << put_result << endl;
            assert (put_result == status_codes::OK);
            
            pair<status_code,value> result {
                do_request (methods::GET,
                            string(GetFixture::addr)
                            + read_entity_admin  + "/"
                            + "TestTable" + "/" + "Audi" + "/" +"*" )};
            CHECK_EQUAL(status_codes::OK, result.first);
            CHECK_EQUAL(status_codes::OK, delete_entity (GetFixture::addr, GetFixture::table, partition, row));
        }
        
        // A test of GET all table entries with partition Audi, wrong table name
        // EXPECT ERROR 404: NOT FOUND
        TEST_FIXTURE(GetFixture, GetSpecific3) {
            string partition {"Audi"};
            string row {"S4"};
            string property {"Horsepower"};
            string prop_val {"300"};
            int put_result {put_entity (GetFixture::addr, GetFixture::table, partition, row, property, prop_val)};
            cerr << "put result " << put_result << endl;
            assert (put_result == status_codes::OK);
            
            pair<status_code,value> result {
                do_request (methods::GET,
                            string(GetFixture::addr)
                            + read_entity_admin  + "/"
                            + "WrongTable" + "/" + "Audi" + "/" +"*" )}; //Input wrong table name
            CHECK_EQUAL(status_codes::NotFound, result.first);
            CHECK_EQUAL(status_codes::OK, delete_entity (GetFixture::addr, GetFixture::table, partition, row));
        }
        
        // A test of GET all table entries with partition Lexus, wrong table name
        // EXPECT ERROR 404: NOT FOUND
        TEST_FIXTURE(GetFixture, GetSpecific4) {
            string partition {"Lexus"};
            string row {"GS"};
            string property {"Torque"};
            string prop_val {"273"};
            int put_result {put_entity (GetFixture::addr, GetFixture::table, partition, row, property, prop_val)};
            cerr << "put result " << put_result << endl;
            assert (put_result == status_codes::OK);
            
            pair<status_code,value> result {
                do_request (methods::GET,
                            string(GetFixture::addr)
                            + read_entity_admin + "/"
                            + "AnotherWrongTable" + "/" + "Lexus" + "/" +"*" )}; //Input wrong table name
            CHECK_EQUAL(status_codes::NotFound, result.first);
            CHECK_EQUAL(status_codes::OK, delete_entity (GetFixture::addr, GetFixture::table, partition, row));
        }
        
        // A test of GET all table entries with partition Lexus, with missing *
        // EXPECT ERROR 400: BAD REQUEST
        TEST_FIXTURE(GetFixture, GetSpecific5) {
            string partition {"Lexus"};
            string row {"IS"};
            string property {"Torque"};
            string prop_val {"273"};
            int put_result {put_entity (GetFixture::addr, GetFixture::table, partition, row, property, prop_val)};
            cerr << "put result " << put_result << endl;
            assert (put_result == status_codes::OK);
            
            pair<status_code,value> result {
                do_request (methods::GET,
                            string(GetFixture::addr)
                            + read_entity_admin + "/"
                            + "TestTable" + "/" + "Lexus" + "/" + "" )}; //Input with no *
            CHECK_EQUAL(status_codes::BadRequest, result.first);
            CHECK_EQUAL(status_codes::OK, delete_entity (GetFixture::addr, GetFixture::table, partition, row));
        }
        
        // A test of GET all table entries with partition Lexus, with missing partition name
        // EXPECT ERROR 400: BAD REQUEST
        TEST_FIXTURE(GetFixture, GetSpecific6) {
            string partition {"Lexus"};
            string row {"IS"};
            string property {"Torque"};
            string prop_val {"273"};
            int put_result {put_entity (GetFixture::addr, GetFixture::table, partition, row, property, prop_val)};
            cerr << "put result " << put_result << endl;
            assert (put_result == status_codes::OK);
            
            pair<status_code,value> result {
                do_request (methods::GET,
                            string(GetFixture::addr)
                            + read_entity_admin + "/"
                            + "TestTable" + "/" +  "/" + "*" )}; //Input with no partition name
            CHECK_EQUAL(status_codes::BadRequest, result.first);
            CHECK_EQUAL(status_codes::OK, delete_entity (GetFixture::addr, GetFixture::table, partition, row));
        }
        
        // A test of GET all table entries with partition Lexus, with missing table name            TEST 7
        // EXPECT ERROR 400: BAD REQUEST
        TEST_FIXTURE(GetFixture, GetSpecific7) {
            string partition {"Apple"};
            string row {"iPhone"};
            string property {"Color"};
            string prop_val {"Gold"};
            int put_result {put_entity (GetFixture::addr, GetFixture::table, partition, row, property, prop_val)};
            cerr << "put result " << put_result << endl;
            assert (put_result == status_codes::OK);
            
            pair<status_code,value> result {
                do_request (methods::GET,
                            string(GetFixture::addr)
                            + read_entity_admin + "/"
                            + "Apple" + "/" + "*" )}; //Input with no table name
            CHECK_EQUAL(status_codes::BadRequest, result.first);
            CHECK_EQUAL(status_codes::OK, delete_entity (GetFixture::addr, GetFixture::table, partition, row));
        }
        
        //BASIC test case for peoblem 2
        TEST_FIXTURE(GetFixture, GetProperty) {
            string partition {"Edmund"};
            string row {"Ottawa"};
            string property {"Born"};
            string prop_val {"2000"};
            int put_result {put_entity (GetFixture::addr, GetFixture::table, partition, row, property, prop_val)};
            cerr << "put result " << put_result << endl;
            assert (put_result == status_codes::OK);
            
            pair<status_code,value> result {
                do_request (methods::GET,
                            string(GetFixture::addr)
                            + read_entity_admin + "/"
                            + string(GetFixture::table)
                            , value::object (vector<pair<string,value>>
                                             {make_pair("Born", value::string("*"))})
                            )};
            
            CHECK_EQUAL(1, result.second.as_array().size());
            CHECK_EQUAL(status_codes::OK, result.first);
            CHECK_EQUAL(status_codes::OK, delete_entity (GetFixture::addr, GetFixture::table, partition, row));
            
        }
        
        //Testing for 2 entities with same property name
        TEST_FIXTURE(GetFixture, GetProperty2) {
            string partition {"Lamar,Kendrick"};
            string row {"USA"};
            string property {"Song"};
            string prop_val {"I"};
            int put_result {put_entity (GetFixture::addr, GetFixture::table, partition, row, property, prop_val)};
            cerr << "put result " << put_result << endl;
            assert (put_result == status_codes::OK);
            
            pair<status_code,value> result {
                do_request (methods::GET,
                            string(GetFixture::addr)
                            + read_entity_admin + "/"
                            + string(GetFixture::table)
                            , value::object (vector<pair<string,value>>
                                             {make_pair("Song", value::string("*"))})
                            )};
            
            CHECK_EQUAL(2, result.second.as_array().size());
            CHECK_EQUAL(status_codes::OK, result.first);
            CHECK_EQUAL(status_codes::OK, delete_entity (GetFixture::addr, GetFixture::table, partition, row));
            
        }
        //Test by getting a fake property name which will return an empty array
        
        TEST_FIXTURE(GetFixture, GetProperty3) {
            string partition {"Miles,Desmond"};
            string row {"USA"};
            string property {"Job"};
            string prop_val {"Assassin"};
            int put_result {put_entity (GetFixture::addr, GetFixture::table, partition, row, property, prop_val)};
            cerr << "put result " << put_result << endl;
            assert (put_result == status_codes::OK);
            
            pair<status_code,value> result {
                do_request (methods::GET,
                            string(GetFixture::addr)
                            + read_entity_admin + "/"
                            + string(GetFixture::table)
                            , value::object (vector<pair<string,value>>
                                             {make_pair("Fake property", value::string("*"))})
                            )};
            
            CHECK_EQUAL(0, result.second.as_array().size());
            CHECK_EQUAL(status_codes::OK, result.first);
            CHECK_EQUAL(status_codes::OK, delete_entity (GetFixture::addr, GetFixture::table, partition, row));
        }
        
        
        //Test GETting items with MULTIPLE properties
        TEST_FIXTURE(GetFixture, GetProperty4) {
            string partition {"Edmund"};
            string row {"Ottawa"};
            string property {"Born"};
            string prop_val {"2000"};
            vector <pair<string,value>> multi_prop {make_pair("Born",value::string("1990")),make_pair("art",value::string("nothing"))};
            int put_result {put_entity_multiple_props (GetFixture::addr, GetFixture::table, partition, row, multi_prop)};
            cerr << "put result " << put_result << endl;
            assert (put_result == status_codes::OK);
            
            pair<status_code,value> result {
                do_request (methods::GET,
                            string(GetFixture::addr)
                            + read_entity_admin + "/"
                            + string(GetFixture::table)
                            , value::object (vector<pair<string,value>>
                                             {make_pair("Born", value::string("*")),make_pair("art",value::string("*"))})
                            )};
            
            CHECK_EQUAL(1, result.second.as_array().size());
            CHECK_EQUAL(status_codes::OK, result.first);
            CHECK_EQUAL(status_codes::OK, delete_entity (GetFixture::addr, GetFixture::table, partition, row));
        }
        
        //Testing for BadRequest
        TEST_FIXTURE(GetFixture, GetProperty5) {
            string partition {"Doe,John"};
            string row {"Ottawa"};
            string property {"Born"};
            string prop_val {"2000"};
            int put_result {put_entity (GetFixture::addr, GetFixture::table, partition, row, property, prop_val)};
            cerr << "put result " << put_result << endl;
            assert (put_result == status_codes::OK);
            
            pair<status_code,value> result {
                do_request (methods::GET,
                            string(GetFixture::addr)
                            + read_entity_admin + "/"
                            + string(GetFixture::table)+ "/" + partition //path size == 2, causing a BadRequest
                            , value::object (vector<pair<string,value>>
                                             {make_pair("Born", value::string("*"))})
                            )};
            
            CHECK_EQUAL(status_codes::BadRequest, result.first);
            CHECK_EQUAL(status_codes::OK, delete_entity (GetFixture::addr, GetFixture::table, partition, row));
        }
        
        //Testing for wrong table name (expected NOT FOUND)
        TEST_FIXTURE(GetFixture, GetProperty6) {
            string partition {"Edmund"};
            string row {"Ottawa"};
            string property {"Born"};
            string prop_val {"2000"};
            int put_result {put_entity (GetFixture::addr, GetFixture::table, partition, row, property, prop_val)};
            cerr << "put result " << put_result << endl;
            assert (put_result == status_codes::OK);
            
            pair<status_code,value> result {
                do_request (methods::GET,
                            string(GetFixture::addr)
                            + read_entity_admin + "/"
                            + "FakeTable"
                            , value::object (vector<pair<string,value>>
                                             {make_pair("Born", value::string("*"))})
                            )};
            
            CHECK_EQUAL(status_codes::NotFound, result.first);
            CHECK_EQUAL(status_codes::OK, delete_entity (GetFixture::addr, GetFixture::table, partition, row));
        }
    }
}
class AuthFixture {
public:
    static constexpr const char* addr {"http://localhost:34568/"};
    static constexpr const char* auth_addr {"http://localhost:34570/"};
    static constexpr const char* userid {"user"};
    static constexpr const char* user_pwd {"user"};
    static constexpr const char* auth_table {"AuthTable"};
    static constexpr const char* auth_table_partition {"Userid"};
    static constexpr const char* auth_pwd_prop {"Password"};
    static constexpr const char* table {"DataTable"};
    static constexpr const char* partition {"USA"};
    static constexpr const char* row {"Franklin,Aretha"};
    static constexpr const char* property {"Song"};
    static constexpr const char* prop_val {"RESPECT"};
    
public:
    AuthFixture() {
        int make_result {create_table(addr, table)};
        cerr << "create result " << make_result << endl;
        if (make_result != status_codes::Created && make_result != status_codes::Accepted) {
            throw std::exception();
        }
        int put_result {put_entity (addr, table, partition, row, property, prop_val)};
        cerr << "put result " << put_result << endl;
        if (put_result != status_codes::OK) {
            throw std::exception();
        }
        // Ensure userid and password in system
        int user_result {put_entity (addr,
                                     auth_table,
                                     auth_table_partition,
                                     userid,
                                     auth_pwd_prop,
                                     user_pwd)};
        cerr << "user auth table insertion result " << user_result << endl;
        if (user_result != status_codes::OK)
            throw std::exception();
        
    }
    
    ~AuthFixture() {
        int del_ent_result {delete_entity (addr, table, partition, row)};
        if (del_ent_result != status_codes::OK) {
            throw std::exception();
        }
    }
};

SUITE(GET_TOKEN){
    //initialize value = demanded value
    //OK
    TEST_FIXTURE(AuthFixture, ReadOnlyAuth1){
        string userid = "user";
        string pwdprop = "Password";
        string pwdval = "user";
        value pasval = build_json_object(vector<pair<string,string>> {make_pair(pwdprop,pwdval)});
        vector<pair<string,value>> pasvec {make_pair(pwdprop,pasval)};
        
        //int put_entity_token(const string& addr, const string& table, const string& userid, const vector<pair<string,value>>& password)
        try {
        int put_result {put_entity_token(AuthFixture::auth_addr,AuthFixture::table,userid,pasvec)};
        cerr<<"put result "<<put_result<<endl;
        CHECK_EQUAL(status_codes::OK, put_result);
        }
        
        catch(const std::exception& e){
            cout<<e.what()<<endl;
        }
        

        
        cout << "Requesting token" << endl;
        pair<status_code,string> token_res {
            get_read_token(AuthFixture::auth_addr,
                           userid,
                           pwdval)};
        cout << "Token response " << token_res.first << endl;
        CHECK_EQUAL (token_res.first, status_codes::OK);
        CHECK_EQUAL(status_codes::OK, delete_entity(AuthFixture::addr,AuthFixture::table,partition,row));
        
    }
    
    
    
     //BadRequest
     //missing userid from the URI
     TEST_FIXTURE(AuthFixture, ReadOnlyAuth2){
     string userid = "Ren";
     string pwdprop = "Password";
     string pwdval = "anarchy";
     
     value pasval = build_json_object(vector<pair<string,string>> {make_pair(pwdprop,pwdval)});
     vector<pair<string,value>> pasvec {make_pair(pwdprop,pasval)};
     
     //int put_entity_token(const string& addr, const string& table, const string& userid, const vector<pair<string,value>>& password)
     int put_result {put_entity_token(AuthFixture::addr,AuthFixture::table,userid,pasvec)};

     cerr<<"put result "<<put_result<<endl;
     assert(put_result == status_codes::OK);
     
     cout << "Requesting token" << endl;
     pair<status_code,string> token_res {
     get_read_token(AuthFixture::auth_addr,
     "",
     pwdval)};
     cout << "Token response " << token_res.first << endl;
     CHECK_EQUAL (token_res.first, status_codes::BadRequest);
     CHECK_EQUAL(status_codes::OK, delete_entity(AuthFixture::addr,AuthFixture::table,partition,row));
     }
     //BadRequest
     //contains no password
     TEST_FIXTURE(AuthFixture, ReadOnlyAuth3){
     string userid = "Ren";
     string pwdprop = "Password";
     string pwdval = "anarchy";
     
     value pasval = build_json_object(vector<pair<string,string>> {make_pair(pwdprop,pwdval)});
     vector<pair<string,value>> pasvec {make_pair(pwdprop,pasval)};
     
     //int put_entity_token(const string& addr, const string& table, const string& userid, const vector<pair<string,value>>& password)
     int put_result {put_entity_token(AuthFixture::addr,AuthFixture::table,userid,pasvec)};

     cerr<<"put result "<<put_result<<endl;
     assert(put_result == status_codes::OK);
     
     cout << "Requesting token" << endl;
     pair<status_code,string> token_res {
     get_read_token(AuthFixture::auth_addr,
     userid,
     "")};
     cout << "Token response " << token_res.first << endl;
     CHECK_EQUAL (token_res.first, status_codes::BadRequest);
     CHECK_EQUAL(status_codes::OK, delete_entity(AuthFixture::addr,AuthFixture::table,partition,row));
     }
     
     //BadRequest
     //more properties other than "Password"
     // ga tau
     TEST_FIXTURE(AuthFixture, ReadOnlyAuth4){
     string userid = "Ren";
     string pwdprop = "Password";
     string pwdval = "anarchy";
     
     value pasval = build_json_object(vector<pair<string,string>> {make_pair(pwdprop,pwdval)});
     vector<pair<string,value>> pasvec {make_pair(pwdprop,pasval)};
     
     //int put_entity_token(const string& addr, const string& table, const string& userid, const vector<pair<string,value>>& password)
     int put_result {put_entity_token(AuthFixture::addr,AuthFixture::table,userid,pasvec)};

     cerr<<"put result "<<put_result<<endl;
     assert(put_result == status_codes::OK);
     
     cout << "Requesting token" << endl;
     pair<status_code,string> token_res {
     get_read_token(AuthFixture::auth_addr,
     userid,
     "otherpassword")};
     cout << "Token response " << token_res.first << endl;
     CHECK_EQUAL (token_res.first, status_codes::BadRequest);
     CHECK_EQUAL(status_codes::OK, delete_entity(AuthFixture::addr,AuthFixture::table,partition,row));
     }
     
     // NotFound belum dikerjain, userid wasnot found in thetable
     TEST_FIXTURE(AuthFixture, ReadOnlyAuth5){
     string userid = "Ren";
     string pwdprop = "Password";
     string pwdval = "anarchy";
     
     value pasval = build_json_object(vector<pair<string,string>> {make_pair(pwdprop,pwdval)});
     vector<pair<string,value>> pasvec {make_pair(pwdprop,pasval)};
     
     //int put_entity_token(const string& addr, const string& table, const string& userid, const vector<pair<string,value>>& password)
     int put_result {put_entity_token(AuthFixture::addr,AuthFixture::table,userid,pasvec)};

     cerr<<"put result "<<put_result<<endl;
     assert(put_result == status_codes::OK);
     
     cout << "Requesting token" << endl;
     pair<status_code,string> token_res {
     get_read_token(AuthFixture::auth_addr,
     "Tonny",
     pwdval)};
     cout << "Token response " << token_res.first << endl;
     CHECK_EQUAL (token_res.first, status_codes::NotFound);
     CHECK_EQUAL(status_codes::OK, delete_entity(AuthFixture::addr,AuthFixture::table,partition,row));
     }
     
     //found but password didnt match
     TEST_FIXTURE(AuthFixture, ReadOnlyAuth6){
     string userid = "Ren";
     string pwdprop = "Password";
     string pwdval = "anarchy";
     
     value pasval = build_json_object(vector<pair<string,string>> {make_pair(pwdprop,pwdval)});
     vector<pair<string,value>> pasvec {make_pair(pwdprop,pasval)};
     
     //int put_entity_token(const string& addr, const string& table, const string& userid, const vector<pair<string,value>>& password)
     int put_result {put_entity_token(AuthFixture::addr,AuthFixture::table,userid,pasvec)};

     cerr<<"put result "<<put_result<<endl;
     assert(put_result == status_codes::OK);
     
     cout << "Requesting token" << endl;
     pair<status_code,string> token_res {
     get_read_token(AuthFixture::auth_addr,
     userid,
     "Nostalgia")};
     cout << "Token response " << token_res.first << endl;
     CHECK_EQUAL (token_res.first, status_codes::NotFound);
     CHECK_EQUAL(status_codes::OK, delete_entity(AuthFixture::addr,AuthFixture::table,partition,row));
     }
    
     
     
     SUITE(UPDATE_TOKEN){
     TEST_FIXTURE(AuthFixture, UpdateAuth1){
     string userid = "Ren";
     string pwdprop = "Password";
     string pwdval = "anarchy";
     
     value pasval = build_json_object(vector<pair<string,string>> {make_pair(pwdprop,pwdval)});
     vector<pair<string,value>> pasvec {make_pair(pwdprop,pasval)};
     
     //int put_entity_token(const string& addr, const string& table, const string& userid, const vector<pair<string,value>>& password)
     int put_result {put_entity_token(AuthFixture::addr,AuthFixture::table,userid,pasvec)};

     cerr<<"put result "<<put_result<<endl;
     assert(put_result == status_codes::OK);
     
     cout << "Requesting token" << endl;
     pair<status_code,string> token_res {
     get_update_token(AuthFixture::auth_addr,
     userid,
     pwdval)};
     cout << "Token response " << token_res.first << endl;
     CHECK_EQUAL (token_res.first, status_codes::OK);
     CHECK_EQUAL(status_codes::OK, delete_entity(AuthFixture::addr,AuthFixture::table,partition,row));
     }
     
     
     }

     
    
     SUITE(UPDATE_AUTH) {
     TEST_FIXTURE(AuthFixture,  PutAuth) {
     
     pair<string,string> added_prop {make_pair(string("born"),string("1942"))};
     
     cout << "Requesting token" << endl;
     pair<status_code,string> token_res {
     get_update_token(AuthFixture::auth_addr,
     AuthFixture::userid,
     AuthFixture::user_pwd)};
     cout << "Token response " << token_res.first << endl;
     CHECK_EQUAL (token_res.first, status_codes::OK);
     
     pair<status_code,value> result {
     do_request (methods::PUT,
     string(AuthFixture::addr)
     + update_entity_auth + "/"
     + AuthFixture::table + "/"
     + token_res.second + "/"
     + AuthFixture::partition + "/"
     + AuthFixture::row,
     value::object (vector<pair<string,value>>
     {make_pair(added_prop.first,
     value::string(added_prop.second))})
     )};
     CHECK_EQUAL(status_codes::OK, result.first);
     
     pair<status_code,value> ret_res {
     do_request (methods::GET,
     string(AuthFixture::addr)
     + read_entity_admin + "/"
     + AuthFixture::table + "/"
     + AuthFixture::partition + "/"
     + AuthFixture::row)};
     CHECK_EQUAL (status_codes::OK, ret_res.first);
     value expect {
     build_json_object (vector<pair<string,string>>
     {added_prop,
     make_pair(string(AuthFixture::property),
     string(AuthFixture::prop_val))}
     )};
     
     cout << AuthFixture::property << endl;
     compare_json_values (expect, ret_res.second);
     }
     }
    
     //Read Authorize and successful
     //OK
     SUITE(GET_AUTH){
     TEST_FIXTURE(AuthFixture, ReadAuth1){
     string partition {"Solasido"};
     string row {"Indonesia"};
     string property {"Location"};
     string prop_val {"Jawa"};
     int put_result {put_entity (AuthFixture::addr, AuthFixture::table, partition, row, property, prop_val)};
     cerr << "put result " << put_result << endl;
     assert (put_result == status_codes::OK);
     
     cout << "Requesting token" << endl;
     pair<status_code,string> token_res {
     get_read_token(AuthFixture::auth_addr,
     AuthFixture::userid,
     AuthFixture::user_pwd)};
     cout << "Token response " << token_res.first << endl;
     CHECK_EQUAL (token_res.first, status_codes::OK);
     
     pair<status_code,value> ret_res {
     do_request (methods::GET,
     string(AuthFixture::addr)
     + read_entity_admin + "/"
     + AuthFixture::table + "/"
     + AuthFixture::partition + "/"
     + AuthFixture::row)};
     CHECK_EQUAL (status_codes::OK, ret_res.first);
     CHECK_EQUAL(status_codes::OK, delete_entity(AuthFixture::addr,AuthFixture::table,partition,row));
     }
    
     
     //table found
     //OK
     TEST_FIXTURE(AuthFixture, ReadAuth2){
     string partition {"Solasido"};
     string row {"Indonesia"};
     string property {"Location"};
     string prop_val {"Jawa"};
     int put_result {put_entity (AuthFixture::addr, AuthFixture::table, partition, row, property, prop_val)};
     cerr << "put result " << put_result << endl;
     assert (put_result == status_codes::OK);
     
     cout << "Requesting token" << endl;
     pair<status_code,string> token_res {
     get_read_token(AuthFixture::auth_addr,
     AuthFixture::userid,
     AuthFixture::user_pwd)};
     cout << "Token response " << token_res.first << endl;
     CHECK_EQUAL (token_res.first, status_codes::OK);
     
     pair<status_code,value> ret_res {
     do_request (methods::GET,
     string(AuthFixture::addr)
     + read_entity_admin + "/"
     + "DataTable" + "/"
     + AuthFixture::partition + "/"
     + AuthFixture::row)};
     CHECK_EQUAL (status_codes::OK, ret_res.first);
     CHECK_EQUAL(status_codes::OK, delete_entity(AuthFixture::addr,AuthFixture::table,partition,row));
     }
     //missing table name
     //NotFound
     TEST_FIXTURE(AuthFixture, ReadAuth3){
     string partition {"Solasido"};
     string row {"Indonesia"};
     string property {"Location"};
     string prop_val {"Jawa"};
     int put_result {put_entity (AuthFixture::addr, AuthFixture::table, partition, row, property, prop_val)};
     cerr << "put result " << put_result << endl;
     assert (put_result == status_codes::OK);
     
     cout << "Requesting token" << endl;
     pair<status_code,string> token_res {
     get_read_token(AuthFixture::auth_addr,
     AuthFixture::userid,
     AuthFixture::user_pwd)};
     cout << "Token response " << token_res.first << endl;
     CHECK_EQUAL (token_res.first, status_codes::OK);
     
     pair<status_code,value> ret_res {
     do_request (methods::GET,
     string(AuthFixture::addr)
     + read_entity_admin + "/"
     + "" + "/"
     + AuthFixture::partition + "/"
     + AuthFixture::row)};
     CHECK_EQUAL (status_codes::NotFound, ret_res.first);
     CHECK_EQUAL(status_codes::OK, delete_entity(AuthFixture::addr,AuthFixture::table,partition,row));
     }
     //missing partition name
     //NotFound
     TEST_FIXTURE(AuthFixture, ReadAuth4){
     string partition {"Solasido"};
     string row {"Indonesia"};
     string property {"Location"};
     string prop_val {"Jawa"};
     int put_result {put_entity (AuthFixture::addr, AuthFixture::table, partition, row, property, prop_val)};
     cerr << "put result " << put_result << endl;
     assert (put_result == status_codes::OK);
     
     cout << "Requesting token" << endl;
     pair<status_code,string> token_res {
     get_read_token(AuthFixture::auth_addr,
     AuthFixture::userid,
     AuthFixture::user_pwd)};
     cout << "Token response " << token_res.first << endl;
     CHECK_EQUAL (token_res.first, status_codes::OK);
     
     pair<status_code,value> ret_res {
     do_request (methods::GET,
     string(AuthFixture::addr)
     + read_entity_admin + "/"
     + AuthFixture::table + "/"
     + "" + "/"
     + AuthFixture::row)};
     CHECK_EQUAL (status_codes::NotFound, ret_res.first);
     CHECK_EQUAL(status_codes::OK, delete_entity(AuthFixture::addr,AuthFixture::table,partition,row));
     }
     //missing row name
     //NotFound
     TEST_FIXTURE(AuthFixture, ReadAuth5){
     string partition {"Solasido"};
     string row {"Indonesia"};
     string property {"Location"};
     string prop_val {"Jawa"};
     int put_result {put_entity (AuthFixture::addr, AuthFixture::table, partition, row, property, prop_val)};
     cerr << "put result " << put_result << endl;
     assert (put_result == status_codes::OK);
     
     cout << "Requesting token" << endl;
     pair<status_code,string> token_res {
     get_read_token(AuthFixture::auth_addr,
     AuthFixture::userid,
     AuthFixture::user_pwd)};
     cout << "Token response " << token_res.first << endl;
     CHECK_EQUAL (token_res.first, status_codes::OK);
     
     pair<status_code,value> ret_res {
     do_request (methods::GET,
     string(AuthFixture::addr)
     + read_entity_admin + "/"
     + AuthFixture::table + "/"
     + AuthFixture::partition + "/"
     + "")};
     CHECK_EQUAL (status_codes::NotFound, ret_res.first);
     CHECK_EQUAL(status_codes::OK, delete_entity(AuthFixture::addr,AuthFixture::table,partition,row));
     }
     //table not found
     //NotFound
     TEST_FIXTURE(AuthFixture, ReadAuth6){
     string partition {"Solasido"};
     string row {"Indonesia"};
     string property {"Location"};
     string prop_val {"Jawa"};
     int put_result {put_entity (AuthFixture::addr, AuthFixture::table, partition, row, property, prop_val)};
     cerr << "put result " << put_result << endl;
     assert (put_result == status_codes::OK);
     
     cout << "Requesting token" << endl;
     pair<status_code,string> token_res {
     get_read_token(AuthFixture::auth_addr,
     AuthFixture::userid,
     AuthFixture::user_pwd)};
     cout << "Token response " << token_res.first << endl;
     CHECK_EQUAL (token_res.first, status_codes::OK);
     
     pair<status_code,value> ret_res {
     do_request (methods::GET,
     string(AuthFixture::addr)
     + read_entity_admin + "/"
     + "FakeTableName" + "/"
     + AuthFixture::partition + "/"
     + AuthFixture::row)};
     CHECK_EQUAL (status_codes::NotFound, ret_res.first);
     CHECK_EQUAL(status_codes::OK, delete_entity(AuthFixture::addr,AuthFixture::table,partition,row));
     }
     //less than 4 param
     //BadRequest
     TEST_FIXTURE(AuthFixture, ReadAuth7){
     string partition {"Solasido"};
     string row {"Indonesia"};
     string property {"Location"};
     string prop_val {"Jawa"};
     int put_result {put_entity (AuthFixture::addr, AuthFixture::table, partition, row, property, prop_val)};
     cerr << "put result " << put_result << endl;
     assert (put_result == status_codes::OK);
     
     cout << "Requesting token" << endl;
     pair<status_code,string> token_res {
     get_read_token(AuthFixture::auth_addr,
     AuthFixture::userid,
     AuthFixture::user_pwd)};
     cout << "Token response " << token_res.first << endl;
     CHECK_EQUAL (token_res.first, status_codes::OK);
     
     pair<status_code,value> ret_res {
     do_request (methods::GET,
     string(AuthFixture::addr)
     + read_entity_admin + "/"
     + AuthFixture::table + "/"
     + AuthFixture::partition)};
     CHECK_EQUAL (status_codes::BadRequest, ret_res.first);
     CHECK_EQUAL(status_codes::OK, delete_entity(AuthFixture::addr,AuthFixture::table,partition,row));
     }
     //token did not authorize access to this entity
     //NotFound
     TEST_FIXTURE(AuthFixture, ReadAuth8){
     string partition {"Solasido"};
     string row {"Indonesia"};
     string property {"Location"};
     string prop_val {"Jawa"};
     int put_result {put_entity (AuthFixture::addr, AuthFixture::table, partition, row, property, prop_val)};
     cerr << "put result " << put_result << endl;
     assert (put_result == status_codes::OK);
     
     cout << "Requesting token" << endl;
     pair<status_code,string> token_res {
     get_read_token(AuthFixture::auth_addr,
     AuthFixture::userid,
     AuthFixture::user_pwd)};
     cout << "Token response " << token_res.first << endl;
     CHECK_EQUAL (token_res.first, status_codes::OK);
     
     pair<status_code,value> ret_res {
     do_request (methods::GET,
     string(AuthFixture::addr)
     + read_entity_admin + "/"
     + AuthFixture::table + "/"
     + AuthFixture::partition)};
     CHECK_EQUAL (status_codes::NotFound, ret_res.first);
     CHECK_EQUAL(status_codes::OK, delete_entity(AuthFixture::addr,AuthFixture::table,partition,row));
     }
     
     }
    
    
     SUITE(TEST_AUTH){
     
     //Change property
     //OK
     TEST_FIXTURE(AuthFixture, SingleAuth){
     string partition {"Sol"};
     string row {"Korea"};
     string property {"Location"};
     string prop_val {"Langley"};
     int put_result {put_entity (AuthFixture::addr, AuthFixture::table, partition, row, property, prop_val)};
     cerr << "put result " << put_result << endl;
     assert (put_result == status_codes::OK);
     
     pair<string,string> added_prop {make_pair(string("Location"),string("Vancouver"))};
     
     cout << "Requesting token" << endl;
     pair<status_code,string> token_res {
     get_update_token(AuthFixture::auth_addr,
     AuthFixture::userid,
     AuthFixture::user_pwd)};
     cout << "Token response " << token_res.first << endl;
     CHECK_EQUAL (token_res.first, status_codes::OK);
     
     pair<status_code,value> result {
     do_request (methods::PUT,
     string(AuthFixture::addr)
     + update_entity_auth + "/"
     + AuthFixture::table + "/"
     + token_res.second + "/"
     + AuthFixture::partition + "/"
     + AuthFixture::row,
     value::object (vector<pair<string,value>>
     {make_pair(added_prop.first,
     value::string(added_prop.second))})
     )};
     CHECK_EQUAL(status_codes::OK, result.first);
     
     pair<status_code,value> ret_res {
     do_request (methods::GET,
     string(AuthFixture::addr)
     + read_entity_admin + "/"
     + AuthFixture::table + "/"
     + AuthFixture::partition + "/"
     + AuthFixture::row)};
     CHECK_EQUAL (status_codes::OK, ret_res.first);
     value expect {
     build_json_object (vector<pair<string,string>>
     {added_prop,
     make_pair(string(AuthFixture::property),
     string(AuthFixture::prop_val))}
     )};
     
     cout << AuthFixture::property << endl;
     compare_json_values (expect, ret_res.second);
     CHECK_EQUAL(status_codes::OK, delete_entity(AuthFixture::addr,AuthFixture::table,partition,row));
     }
     
     //Updating same property
     //OK
     TEST_FIXTURE(AuthFixture, SingleAuth2){
     string partition {"Sol"};
     string row {"Korea"};
     string property {"Location"};
     string prop_val {"Langley"};
     int put_result {put_entity (AuthFixture::addr, AuthFixture::table, partition, row, property, prop_val)};
     cerr << "put result " << put_result << endl;
     assert (put_result == status_codes::OK);
     
     pair<string,string> added_prop {make_pair(string("Location"),string("Langley"))};
     
     cout << "Requesting token" << endl;
     pair<status_code,string> token_res {
     get_update_token(AuthFixture::auth_addr,
     AuthFixture::userid,
     AuthFixture::user_pwd)};
     cout << "Token response " << token_res.first << endl;
     CHECK_EQUAL (token_res.first, status_codes::OK);
     
     pair<status_code,value> result {
     do_request (methods::PUT,
     string(AuthFixture::addr)
     + update_entity_auth + "/"
     + AuthFixture::table + "/"
     + token_res.second + "/"
     + AuthFixture::partition + "/"
     + AuthFixture::row,
     value::object (vector<pair<string,value>>
     {make_pair(added_prop.first,
     value::string(added_prop.second))})
     )};
     CHECK_EQUAL(status_codes::OK, result.first);
     
     pair<status_code,value> ret_res {
     do_request (methods::GET,
     string(AuthFixture::addr)
     + read_entity_admin + "/"
     + AuthFixture::table + "/"
     + AuthFixture::partition + "/"
     + AuthFixture::row)};
     CHECK_EQUAL (status_codes::OK, ret_res.first);
     value expect {
     build_json_object (vector<pair<string,string>>
     {added_prop,
     make_pair(string(AuthFixture::property),
     string(AuthFixture::prop_val))}
     )};
     
     cout << AuthFixture::property << endl;
     compare_json_values (expect, ret_res.second);
     CHECK_EQUAL(status_codes::OK, delete_entity(AuthFixture::addr,AuthFixture::table,partition,row));
     }
     //From No property and update it to a property
     //OK
     TEST_FIXTURE(AuthFixture, SingleAuth3){
     string partition {"Sol"};
     string row {"Korea"};
     string property {"Location"};
     string prop_val {""};
     int put_result {put_entity (AuthFixture::addr, AuthFixture::table, partition, row, property, prop_val)};
     cerr << "put result " << put_result << endl;
     assert (put_result == status_codes::OK);
     
     pair<string,string> added_prop {make_pair(string("Location"),string("Langley"))};
     
     cout << "Requesting token" << endl;
     pair<status_code,string> token_res {
     get_update_token(AuthFixture::auth_addr,
     AuthFixture::userid,
     AuthFixture::user_pwd)};
     cout << "Token response " << token_res.first << endl;
     CHECK_EQUAL (token_res.first, status_codes::OK);
     
     pair<status_code,value> result {
     do_request (methods::PUT,
     string(AuthFixture::addr)
     + update_entity_auth + "/"
     + AuthFixture::table + "/"
     + token_res.second + "/"
     + AuthFixture::partition + "/"
     + AuthFixture::row,
     value::object (vector<pair<string,value>>
     {make_pair(added_prop.first,
     value::string(added_prop.second))})
     )};
     CHECK_EQUAL(status_codes::OK, result.first);
     
     pair<status_code,value> ret_res {
     do_request (methods::GET,
     string(AuthFixture::addr)
     + read_entity_admin + "/"
     + AuthFixture::table + "/"
     + AuthFixture::partition + "/"
     + AuthFixture::row)};
     CHECK_EQUAL (status_codes::OK, ret_res.first);
     value expect {
     build_json_object (vector<pair<string,string>>
     {added_prop,
     make_pair(string(AuthFixture::property),
     string(AuthFixture::prop_val))}
     )};
     
     cout << AuthFixture::property << endl;
     compare_json_values (expect, ret_res.second);
     CHECK_EQUAL(status_codes::OK, delete_entity(AuthFixture::addr,AuthFixture::table,partition,row));
     }
     //expecting 4 param but less than 4 were provided
     //BadRequest
     TEST_FIXTURE(AuthFixture, SingleAuth4){
     string partition {"Sol"};
     string row {"Korea"};
     string property {"Location"};
     string prop_val {"Surrey"};
     int put_result {put_entity (AuthFixture::addr, AuthFixture::table, partition, row, property, prop_val)};
     cerr << "put result " << put_result << endl;
     assert (put_result == status_codes::OK);
     
     pair<string,string> added_prop {make_pair(string("Location"),string("Langley"))};
     
     cout << "Requesting token" << endl;
     pair<status_code,string> token_res {
     get_update_token(AuthFixture::auth_addr,
     AuthFixture::userid,
     AuthFixture::user_pwd)};
     cout << "Token response " << token_res.first << endl;
     CHECK_EQUAL (token_res.first, status_codes::OK);
     
     pair<status_code,value> result {
     do_request (methods::PUT,
     string(AuthFixture::addr)
     + update_entity_auth + "/"
     + AuthFixture::table + "/"
     + token_res.second + "/"
     + AuthFixture::partition,
     value::object (vector<pair<string,value>>
     {make_pair(added_prop.first,
     value::string(added_prop.second))})
     )};
     CHECK_EQUAL(status_codes::BadRequest, result.first);
     
     pair<status_code,value> ret_res {
     do_request (methods::GET,
     string(AuthFixture::addr)
     + read_entity_admin + "/"
     + AuthFixture::table + "/"
     + AuthFixture::partition + "/"
     + AuthFixture::row)};
     CHECK_EQUAL (status_codes::OK, ret_res.first);
     value expect {
     build_json_object (vector<pair<string,string>>
     {added_prop,
     make_pair(string(AuthFixture::property),
     string(AuthFixture::prop_val))}
     )};
     
     cout << AuthFixture::property << endl;
     compare_json_values (expect, ret_res.second);
     CHECK_EQUAL(status_codes::OK, delete_entity(AuthFixture::addr,AuthFixture::table,partition,row));
     }
     
     
     //entity exist but token is for read and not for update
     //Forbidden
     //belum selesai
     TEST_FIXTURE(AuthFixture, SingleAuth5){
     string partition {"Sol"};
     string row {"Korea"};
     string property {"Location"};
     string prop_val {"Surrey"};
     int put_result {put_entity (AuthFixture::addr, AuthFixture::table, partition, row, property, prop_val)};
     cerr << "put result " << put_result << endl;
     assert (put_result == status_codes::OK);
     
     pair<string,string> added_prop {make_pair(string("Location"),string("Langley"))};
     
     cout << "Requesting token" << endl;
     pair<status_code,string> token_res {
     get_read_token(AuthFixture::auth_addr,
     AuthFixture::userid,
     AuthFixture::user_pwd)};
     cout << "Token response " << token_res.first << endl;
     CHECK_EQUAL (token_res.first, status_codes::OK);
     
     pair<status_code,value> result {
     do_request (methods::PUT,
     string(AuthFixture::addr)
     + update_entity_auth + "/"
     + AuthFixture::table + "/"
     + token_res.second + "/"
     + AuthFixture::partition + "/"
     + AuthFixture::row + "/",
     value::object (vector<pair<string,value>>
     {make_pair(added_prop.first,
     value::string(added_prop.second))})
     )};
     CHECK_EQUAL(status_codes::OK, result.first);
     
     pair<status_code,value> ret_res {
     do_request (methods::GET,
     string(AuthFixture::addr)
     + read_entity_admin + "/"
     + AuthFixture::table + "/"
     + AuthFixture::partition + "/"
     + AuthFixture::row)};
     CHECK_EQUAL (status_codes::OK, ret_res.first);
     value expect {
     build_json_object (vector<pair<string,string>>
     {added_prop,
     make_pair(string(AuthFixture::property),
     string(AuthFixture::prop_val))}
     )};
     
     cout << AuthFixture::property << endl;
     compare_json_values (expect, ret_res.second);
     CHECK_EQUAL(status_codes::OK, delete_entity(AuthFixture::addr,AuthFixture::table,partition,row));
     }
     //Table was not found
     //NotFound
     TEST_FIXTURE(AuthFixture, SingleAuth6){
     string partition {"Sol"};
     string row {"Korea"};
     string property {"Location"};
     string prop_val {"Burnaby"};
     int put_result {put_entity (AuthFixture::addr, AuthFixture::table, partition, row, property, prop_val)};
     cerr << "put result " << put_result << endl;
     assert (put_result == status_codes::OK);
     
     pair<string,string> added_prop {make_pair(string("Location"),string("Langley"))};
     
     cout << "Requesting token" << endl;
     pair<status_code,string> token_res {
     get_update_token(AuthFixture::auth_addr,
     AuthFixture::userid,
     AuthFixture::user_pwd)};
     cout << "Token response " << token_res.first << endl;
     CHECK_EQUAL (token_res.first, status_codes::OK);
     
     pair<status_code,value> result {
     do_request (methods::PUT,
     string(AuthFixture::addr)
     + update_entity_auth + "/"
     + "FakeTableName" + "/"
     + token_res.second + "/"
     + AuthFixture::partition + "/"
     + AuthFixture::row,
     value::object (vector<pair<string,value>>
     {make_pair(added_prop.first,
     value::string(added_prop.second))})
     )};
     CHECK_EQUAL(status_codes::NotFound, result.first);
     
     pair<status_code,value> ret_res {
     do_request (methods::GET,
     string(AuthFixture::addr)
     + read_entity_admin + "/"
     + AuthFixture::table + "/"
     + AuthFixture::partition + "/"
     + AuthFixture::row)};
     CHECK_EQUAL (status_codes::OK, ret_res.first);
     value expect {
     build_json_object (vector<pair<string,string>>
     {added_prop,
     make_pair(string(AuthFixture::property),
     string(AuthFixture::prop_val))}
     )};
     
     cout << AuthFixture::property << endl;
     compare_json_values (expect, ret_res.second);
     CHECK_EQUAL(status_codes::OK, delete_entity(AuthFixture::addr,AuthFixture::table,partition,row));
     }
     //No entity with this partition name
     //NotFound
     TEST_FIXTURE(AuthFixture, SingleAuth7){
     string partition {"Sol"};
     string row {"Korea"};
     string property {"Location"};
     string prop_val {"Burnaby"};
     int put_result {put_entity (AuthFixture::addr, AuthFixture::table, partition, row, property, prop_val)};
     cerr << "put result " << put_result << endl;
     assert (put_result == status_codes::OK);
     
     pair<string,string> added_prop {make_pair(string("Location"),string("Langley"))};
     
     cout << "Requesting token" << endl;
     pair<status_code,string> token_res {
     get_update_token(AuthFixture::auth_addr,
     AuthFixture::userid,
     AuthFixture::user_pwd)};
     cout << "Token response " << token_res.first << endl;
     CHECK_EQUAL (token_res.first, status_codes::OK);
     
     pair<status_code,value> result {
     do_request (methods::PUT,
     string(AuthFixture::addr)
     + update_entity_auth + "/"
     + AuthFixture::table + "/"
     + token_res.second + "/"
     + "FakePartitionName" + "/"
     + AuthFixture::row,
     value::object (vector<pair<string,value>>
     {make_pair(added_prop.first,
     value::string(added_prop.second))})
     )};
     CHECK_EQUAL(status_codes::NotFound, result.first);
     
     pair<status_code,value> ret_res {
     do_request (methods::GET,
     string(AuthFixture::addr)
     + read_entity_admin + "/"
     + AuthFixture::table + "/"
     + AuthFixture::partition + "/"
     + AuthFixture::row)};
     CHECK_EQUAL (status_codes::OK, ret_res.first);
     value expect {
     build_json_object (vector<pair<string,string>>
     {added_prop,
     make_pair(string(AuthFixture::property),
     string(AuthFixture::prop_val))}
     )};
     
     cout << AuthFixture::property << endl;
     compare_json_values (expect, ret_res.second);
     CHECK_EQUAL(status_codes::OK, delete_entity(AuthFixture::addr,AuthFixture::table,partition,row));
     }
     
     //No entity with this row name
     //NotFound
     TEST_FIXTURE(AuthFixture, SingleAuth8){
     string partition {"Sol"};
     string row {"Korea"};
     string property {"Location"};
     string prop_val {"Burnaby"};
     int put_result {put_entity (AuthFixture::addr, AuthFixture::table, partition, row, property, prop_val)};
     cerr << "put result " << put_result << endl;
     assert (put_result == status_codes::OK);
     
     pair<string,string> added_prop {make_pair(string("Location"),string("Langley"))};
     
     cout << "Requesting token" << endl;
     pair<status_code,string> token_res {
     get_update_token(AuthFixture::auth_addr,
     AuthFixture::userid,
     AuthFixture::user_pwd)};
     cout << "Token response " << token_res.first << endl;
     CHECK_EQUAL (token_res.first, status_codes::OK);
     
     pair<status_code,value> result {
     do_request (methods::PUT,
     string(AuthFixture::addr)
     + update_entity_auth + "/"
     + AuthFixture::table + "/"
     + token_res.second + "/"
     + AuthFixture::partition + "/"
     + "FakeRowName",
     value::object (vector<pair<string,value>>
     {make_pair(added_prop.first,
     value::string(added_prop.second))})
     )};
     CHECK_EQUAL(status_codes::NotFound, result.first);
     
     pair<status_code,value> ret_res {
     do_request (methods::GET,
     string(AuthFixture::addr)
     + read_entity_admin + "/"
     + AuthFixture::table + "/"
     + AuthFixture::partition + "/"
     + AuthFixture::row)};
     CHECK_EQUAL (status_codes::OK, ret_res.first);
     value expect {
     build_json_object (vector<pair<string,string>>
     {added_prop,
     make_pair(string(AuthFixture::property),
     string(AuthFixture::prop_val))}
     )};
     
     cout << AuthFixture::property << endl;
     compare_json_values (expect, ret_res.second);
     CHECK_EQUAL(status_codes::OK, delete_entity(AuthFixture::addr,AuthFixture::table,partition,row));
     }
     /*
     // //token did not authorize access to this entity
     // //NotFound
     // //Different Entity??
          TEST_FIXTURE(AuthFixture, SingleAuth9){
          string partition {"Sol"};
          string row {"Korea"};
          string property {"Location"};
          string prop_val {"Burnaby"};
          int put_result {put_entity (AuthFixture::addr, AuthFixture::table, partition, row, property, prop_val)};
          cerr << "put result " << put_result << endl;
          assert (put_result == status_codes::OK);
     
        pair<string,string> added_prop {make_pair(string("Location"),string("Langley"))};
     
        cout << "Requesting token" << endl;
        pair<status_code,string> token_res {
          get_update_token(AuthFixture::auth_addr,
                           AuthFixture::userid,
                           AuthFixture::user_pwd)};
        cout << "Token response " << token_res.first << endl;
        CHECK_EQUAL (token_res.first, status_codes::OK);
     
        pair<status_code,value> result {
          do_request (methods::PUT,
                      string(AuthFixture::addr)
                      + update_entity_auth + "/"
                      + AuthFixture::table + "/"
                      + token_res.second + "/"
                      + AuthFixture::partition + "/"
                      + AuthFixture::row,
                      value::object (vector<pair<string,value>>
                                       {make_pair(added_prop.first,
                                                  value::string(added_prop.second))})
                      )};
        CHECK_EQUAL(status_codes::OK, result.first);
     
        pair<status_code,value> ret_res {
          do_request (methods::GET,
                      string(AuthFixture::addr)
                      + read_entity_admin + "/"
                      + AuthFixture::table + "/"
                      + AuthFixture::partition + "/"
                      + AuthFixture::row)};
        CHECK_EQUAL (status_codes::OK, ret_res.first);
        value expect {
          build_json_object (vector<pair<string,string>>
                            {added_prop,
                              make_pair(string(AuthFixture::property),
                                        string(AuthFixture::prop_val))}
     
        cout << AuthFixture::property << endl;
        compare_json_values (expect, ret_res.second);
      CHECK_EQUAL(status_codes::OK, delete_entity(AuthFixture::addr,AuthFixture::table,partition,row));
     }
     
*/
     }
}