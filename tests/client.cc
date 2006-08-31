#include <stdlib.h>
#include <openssl/md5.h>
#include <iostream>
#include <memory>
#include <boost/test/test_tools.hpp>
#include <boost/test/unit_test.hpp>
#include "libiqxmlrpc/libiqxmlrpc.h"
#include "libiqxmlrpc/http_client.h"
#include "client_common.h"
#include "client_opts.h"

using namespace boost::unit_test_framework;
using namespace iqxmlrpc;

// Global
Client_opts test_config;
Client_base* test_client = 0;

void stop_server()
{
  BOOST_REQUIRE(test_client);
  Stop_server_proxy stop(test_client);
  Response retval( stop() );
  BOOST_REQUIRE(!retval.is_fault());
}

void introspection_test()
{
  BOOST_REQUIRE(test_client);
  Introspection_proxy introspect(test_client);
  Response retval( introspect() );
  BOOST_REQUIRE_MESSAGE(!retval.is_fault(), retval.fault_string());

  BOOST_MESSAGE("system.listMethods output:");
  const Value& v = retval.value();
  for (Array::const_iterator i = v.arr_begin(); i != v.arr_end(); ++i)
  {
    BOOST_MESSAGE("\t" + i->get_string());
  }
}

void echo_test()
{
  BOOST_REQUIRE(test_client);
  Echo_proxy echo(test_client);
  Response retval(echo("Hello"));
  BOOST_CHECK(retval.value().get_string() == "Hello");
}

void get_file_test()
{
  BOOST_REQUIRE(test_client);
  Get_file_proxy get_file(test_client);
  Response retval( get_file(1048576) ); // request 1Mb 

  const Value& v = retval.value();
  const Binary_data& d = v["data"];
  const Binary_data& m = v["md5"];

  typedef const unsigned char md5char;
  typedef const char strchar;
  unsigned char md5[16];

  MD5(reinterpret_cast<md5char*>(d.get_data().data()),
    d.get_data().length(), md5);
  std::auto_ptr<Binary_data> gen_md5( Binary_data::from_data(
    reinterpret_cast<strchar*>(md5), sizeof(md5)) );

  BOOST_MESSAGE("Recieved MD5:   " + m.get_base64());
  BOOST_MESSAGE("Calculated MD5: " + gen_md5->get_base64());
  BOOST_WARN_MESSAGE(false, "TODO: Binary_data::operator ==(const Binary_data&)");
  BOOST_CHECK(gen_md5->get_base64() == m.get_base64());
}

test_suite* init_unit_test_suite(int argc, char* argv[])
{
  try {
    test_config.configure(argc, argv);
    test_client = test_config.client_factory()->create();
   
    test_suite* test = BOOST_TEST_SUITE("Client test");
    test->add( BOOST_TEST_CASE(&introspection_test) );
    test->add( BOOST_TEST_CASE(&echo_test) );
    test->add( BOOST_TEST_CASE(&get_file_test) );

    if (test_config.stop_server())
      test->add( BOOST_TEST_CASE(&stop_server) );

    return test;
  }
  catch (const std::exception& e)
  {
    std::cerr << e.what() << std::endl;
    return 0;
  }
}