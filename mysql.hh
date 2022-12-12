/// @file
/// @brief schema and dut classes for MySQL
/// @company www.greatopensource.com

#ifndef MYSQL_HH
#define MYSQL_HH

#include "mysql.h"

#include <sstream>
#include <string>

#include "schema.hh"
#include "relmodel.hh"
#include "dut.hh"
#include "log.hh"

struct mysql_connection {
  MYSQL mysql;
  mysql_connection(std::string &conninfo);
  ~mysql_connection();

  void parse_connection_string(std::string &conninfo);

  std::string host = "127.0.0.1";
  int port = 3306;
  std::string db = "test";
  std::string user = "root";
  std::string password = "";
};

struct schema_mysql : schema, mysql_connection {
  schema_mysql(std::string &conninfo, bool no_catalog);
  virtual std::string quote_name(const std::string &id) {
    std::stringstream ss;
    ss << "`" << id << "`";
    return ss.str();
  }
};

struct dut_mysql : dut_base, mysql_connection {
  virtual void test(const std::string &stmt);
  dut_mysql(std::string &conninfo);
};

#endif
