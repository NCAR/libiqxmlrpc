//  Libiqnet + Libiqxmlrpc - an object-oriented XML-RPC solution.
//  Copyright (C) 2004 Anton Dedov
//  
//  This library is free software; you can redistribute it and/or
//  modify it under the terms of the GNU Lesser General Public
//  License as published by the Free Software Foundation; either
//  version 2.1 of the License, or (at your option) any later version.
//  
//  This library is distributed in the hope that it will be useful,
//  but WITHOUT ANY WARRANTY; without even the implied warranty of
//  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
//  Lesser General Public License for more details.
//  
//  You should have received a copy of the GNU Lesser General Public
//  License along with this library; if not, write to the Free Software
//  Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307  USA
//  
//  $Id: server.cc,v 1.15 2004-10-15 09:42:55 adedov Exp $

#include <memory>
#include "reactor.h"
#include "server.h"
#include "request.h"
#include "response.h"

using namespace iqxmlrpc;


Server_connection::Server_connection():
  server(0),
  read_buf_sz(1024),
  read_buf(new char[1024])
{
}


Server_connection::~Server_connection()
{
  delete[] read_buf;
}


void Server_connection::set_read_sz( unsigned rsz )
{
  delete[] read_buf;
  read_buf_sz = rsz;
  read_buf = new char[read_buf_sz];
}


http::Packet* Server_connection::read_request( const std::string& s )
{
  try 
  {
    preader.set_max_size( server->get_max_request_sz() );
    return preader.read_packet(s);
  }
  catch( const http::Malformed_packet& )
  {
    throw http::Bad_request();
  }
}


void Server_connection::schedule_response( http::Packet* pkt )
{
  response = pkt->dump();
  delete pkt;
}


//-----------------------------------------------------------------------------
Server::Server( int p, Executor_fabric_base* f ):
  exec_fabric(f),
  port(p),
  reactor( f->create_lock() ),
  conn_fabric(0),
  acceptor(0),
  firewall(0),
  exit_flag(false),
  log(0),
  max_req_sz(0)
{
}


Server::~Server()
{
  delete acceptor;
  delete conn_fabric;
  delete exec_fabric;
}


void Server::enable_introspection()
{
  register_method<List_methods_m>( "system.listMethods" );
  register_method<Method_signature_m>( "system.methodSignature" );
  register_method<Method_help_m>( "system.methodHelp" );
}


void Server::log_errors( std::ostream* log_ )
{
  log = log_;
}


void Server::set_max_request_sz( unsigned sz )
{
  max_req_sz = sz;
}


void Server::log_err_msg( const std::string& msg )
{
  if( log )
    *log << msg << std::endl;
}


void Server::schedule_execute( http::Packet* pkt, Server_connection* conn )
{
  Executor* executor = 0;

  try {
    std::auto_ptr<http::Packet> packet(pkt);
    std::auto_ptr<Request> req( parse_request( packet->content() ) );
    Method* meth = disp.create_method( req->get_name() );
    executor = exec_fabric->create( meth, this, conn );
    executor->execute( req->get_params() );
  }
  catch( const iqxmlrpc::Exception& e )
  {
    log_err_msg( std::string("Server: ") + e.what() );
    Response err_r( e.code(), e.what() );
    schedule_response( err_r, conn, executor );
  }
  catch( const std::exception& e )
  {
    log_err_msg( std::string("Server: ") + e.what() );
    Response err_r( -32500 /*application error*/, e.what() );
    schedule_response( err_r, conn, executor );
  }
  catch( ... )
  {
    log_err_msg( "Server: Unknown exception" );
    Response err_r( -32500  /*application error*/, "Unknown Error" );
    schedule_response( err_r, conn, executor );
  }
}


void Server::schedule_response( 
  const Response& resp, Server_connection* conn, Executor* exec )
{
  std::auto_ptr<Executor> executor_to_delete(exec);

  std::auto_ptr<xmlpp::Document> xmldoc( resp.to_xml() );
  std::string resp_str = xmldoc->write_to_string_formatted( "utf-8" );
  
  http::Packet *packet = new http::Packet( new http::Response_header(), resp_str );
  conn->schedule_response( packet );
}
