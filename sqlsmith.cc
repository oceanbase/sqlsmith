#include "config.h"

#include <iostream>
#include <fstream>
#include <chrono>

#ifndef HAVE_BOOST_REGEX
#include <regex>
#else
#include <boost/regex.hpp>
using boost::regex;
using boost::smatch;
using boost::regex_match;
#endif

#include <thread>
#include <typeinfo>
#include <ctime>

#include "random.hh"
#include "grammar.hh"
#include "relmodel.hh"
#include "schema.hh"
#include "gitrev.h"

#include "log.hh"
#include "dump.hh"
#include "impedance.hh"
#include "dut.hh"

#ifdef HAVE_LIBSQLITE3
#include "sqlite.hh"
#endif

#ifdef HAVE_MONETDB
#include "monetdb.hh"
#endif

#include "mysql.hh"

#include "postgres.hh"

using namespace std;

using namespace std::chrono;

extern "C" {
#include <stdlib.h>
#include <unistd.h>
#include <signal.h>
}

/* make the cerr logger globally accessible so we can emit one last
   report on SIGINT */
cerr_logger *global_cerr_logger;

extern "C" void cerr_log_handler(int)
{
  if (global_cerr_logger)
    global_cerr_logger->report();
  exit(1);
}

string convert_time(int duration)
{
  int hour = 0, minute = 0, second = 0;
  while(true){
    if(duration >= 3600){
      hour += 1;
      duration -= 3600;
    }
    else if(duration >= 60){
      minute += 1;
      duration -= 60;
    }
    else{
      second = duration;
      break;
    }
  }
  string ans = to_string(hour) + "小时" + to_string(minute) + "分" + to_string(second) + "秒";
  return ans;
}

int main(int argc, char *argv[])
{
  cerr << PACKAGE_NAME " " GITREV << endl;

  time_t start_time = time(0);  
  std::map<pair<int, string>, long> bugs;
  std::map<pair<int, string>, vector<string>> sql_arr;
  long bugs_amount = 0;

  map<string,string> options;
  regex optregex("--(help|log-to|verbose|target|sqlite|monetdb|mysql|version|dump-all-graphs|dump-all-queries|seed|dry-run|max-queries|rng-state|exclude-catalog)(?:=((?:.|\n)*))?");
  
  for(char **opt = argv+1 ;opt < argv+argc; opt++) {
    smatch match;
    string s(*opt);
    if (regex_match(s, match, optregex)) {
      options[string(match[1])] = match[2];
    } else {
      cerr << "Cannot parse option: " << *opt << endl;
      options["help"] = "";
    }
  }

  if (options.count("help")) {
    cerr <<
      "    --target=connstr     postgres database to send queries to" << endl <<
#ifdef HAVE_LIBSQLITE3
      "    --sqlite=URI         SQLite database to send queries to" << endl <<
#endif
#ifdef HAVE_MONETDB
      "    --monetdb=connstr    MonetDB database to send queries to" <<endl <<
#endif
      "    --mysql=URI    	 MySQL database to send queries to" <<endl <<
      "    --log-to=connstr     log errors to postgres database" << endl <<
      "    --seed=int           seed RNG with specified int instead of PID" << endl <<
      "    --dump-all-queries   print queries as they are generated" << endl <<
      "    --dump-all-graphs    dump generated ASTs" << endl <<
      "    --dry-run            print queries instead of executing them" << endl <<
      "    --exclude-catalog    don't generate queries using catalog relations" << endl <<
      "    --max-queries=long   terminate after generating this many queries" << endl <<
      "    --rng-state=string    deserialize dumped rng state" << endl <<
      "    --verbose            emit progress output" << endl <<
      "    --version            print version information and exit" << endl <<
      "    --help               print available command line options and exit" << endl;
    return 0;
  } else if (options.count("version")) {
    return 0;
  }

  try
    {
      shared_ptr<schema> schema;
      if (options.count("sqlite")) {
#ifdef HAVE_LIBSQLITE3
	schema = make_shared<schema_sqlite>(options["sqlite"], options.count("exclude-catalog"));
#else
	cerr << "Sorry, " PACKAGE_NAME " was compiled without SQLite support." << endl;
	return 1;
#endif
      }
      else if(options.count("monetdb")) {
#ifdef HAVE_MONETDB
	schema = make_shared<schema_monetdb>(options["monetdb"]);
#else
	cerr << "Sorry, " PACKAGE_NAME " was compiled without MonetDB support." << endl;
	return 1;
#endif
			}
	  	else if(options.count("mysql")) {
	schema = make_shared<schema_mysql>(options["mysql"], options.count("exclude-catalog"));
      }
      else
	schema = make_shared<schema_pqxx>(options["target"], options.count("exclude-catalog"));

      scope scope;
      long queries_generated = 0;
      schema->fill_scope(scope);

      if (options.count("rng-state")) {
	   istringstream(options["rng-state"]) >> smith::rng;
      } else {
	   smith::rng.seed(options.count("seed") ? stoi(options["seed"]) : getpid());
      }

      vector<shared_ptr<logger> > loggers;

      loggers.push_back(make_shared<impedance_feedback>());

      if (options.count("log-to"))
	loggers.push_back(make_shared<pqxx_logger>(
	     options.count("sqlite") ? options["sqlite"] : options["target"],
	     options["log-to"], *schema));

      if (options.count("verbose")) {
	auto l = make_shared<cerr_logger>();
	global_cerr_logger = &*l;
	loggers.push_back(l);
	signal(SIGINT, cerr_log_handler);
      }
      
      if (options.count("dump-all-graphs"))
	loggers.push_back(make_shared<ast_logger>());

      if (options.count("dump-all-queries"))
	loggers.push_back(make_shared<query_dumper>());

      if (options.count("dry-run")) {
	while (1) {
	  shared_ptr<prod> gen = statement_factory(&scope);
	  gen->out(cout);
	  for (auto l : loggers)
	    l->generated(*gen);
	  cout << ";" << endl;
	  queries_generated++;

	  if (options.count("max-queries")
	      && (queries_generated >= stol(options["max-queries"])))
	      return 0;
	}
      }

      shared_ptr<dut_base> dut;
      
      if (options.count("sqlite")) {
#ifdef HAVE_LIBSQLITE3
	dut = make_shared<dut_sqlite>(options["sqlite"]);
#else
	cerr << "Sorry, " PACKAGE_NAME " was compiled without SQLite support." << endl;
	return 1;
#endif
      }
      else if(options.count("monetdb")) {
#ifdef HAVE_MONETDB	   
	dut = make_shared<dut_monetdb>(options["monetdb"]);
#else
	cerr << "Sorry, " PACKAGE_NAME " was compiled without MonetDB support." << endl;
	return 1;
#endif
			}
      else if(options.count("mysql")) {
	dut = make_shared<dut_mysql>(options["mysql"]);
      }
      else
	dut = make_shared<dut_libpq>(options["target"]);

      while (1) /* Loop to recover connection loss */
      {
	try {
            while (1) { /* Main loop */

	    if (options.count("max-queries")
		&& (++queries_generated > stol(options["max-queries"]))) {
	      if (global_cerr_logger)
		global_cerr_logger->report();
	      
              cerr << "The number of bugs found in this execution is " << bugs_amount << endl;
	      cerr << "number" << "\t" <<  "err_no" << "\t" << "type of error" << endl;
	      for (auto bug : bugs)
  		cerr << bug.second << "\t" << bug.first.first << "\t" << bug.first.second << endl;
	      ofstream outfile;
	      outfile.open("bug_log.txt");
	      outfile << "err_no" << "\t" << "err_info" << "\t" << "sql_info" << endl;
	      for(auto error : sql_arr){
		     for(auto sql_info : error.second){
			    outfile << error.first.first << "\t" << error.first.second << "\t";
			    outfile << sql_info << endl << endl;
		    }
		    outfile << endl << endl;
		}
	      outfile.close();

              time_t end_time = time(0);
              int duration = static_cast<int>(end_time - start_time);
              cerr << "本次测试结束，累计耗时" << convert_time(duration) << endl;
	      return 0;
	    }
	    
	    /* Invoke top-level production to generate AST */
	    shared_ptr<prod> gen = statement_factory(&scope);

	    for (auto l : loggers)
	      l->generated(*gen);

            if(global_cerr_logger && (10*global_cerr_logger->columns-1) ==
                global_cerr_logger->queries%(10*global_cerr_logger->columns)){
              time_t end_time = time(0);
              long target = stol(options["max-queries"]);
              double duration = static_cast<double>(end_time - start_time);
              double pred_time = duration/queries_generated * (target - queries_generated);
              cerr << "本次测试已运行" << convert_time((int)duration) << ",";
              cerr << "预计还需运行" << convert_time((int)pred_time) << endl;
            }            
	  
	    /* Generate SQL from AST */
	    ostringstream s;
	    gen->out(s);

	    /* Try to execute it */
	    try {
	      dut->test(s.str());
	      for (auto l : loggers)
		l->executed(*gen);
	    } catch (const dut::failure &e) {
	      for (auto l : loggers)
		try {
		  l->error(*gen, e);
		} catch (runtime_error &e) {
		  cerr << endl << "log failed: " << typeid(*l).name() << ": "
		       << e.what() << endl;
		}
	      if ((dynamic_cast<const dut::broken *>(&e))) {
		/* re-throw to outer loop to recover session. */
		throw;
	      }
	    }
	    catch(const std::runtime_error &e){
		  string err_msg = e.what();
                  int index1 = err_msg.find_first_of("::");
                  int index2 = err_msg.find_last_of("::") - 1;
                  int err_no = stoi(err_msg.substr(0, index1));
                  string err_info = err_msg.substr(index1+2, index2-index1-2);
                  string sql_info = err_msg.substr(index2+2);
                  bugs[make_pair(err_no, err_info)]++;
                  bugs_amount++;
                  sql_arr[make_pair(err_no, err_info)].push_back(sql_info);
                  cerr << err_info  << endl;
		  if(err_no == 2013 || err_no == 2006){
		    dut = make_shared<dut_mysql>(options["mysql"]);
		}
	    }
	  }
	}
	catch (const dut::broken &e) {
	  /* Give server some time to recover. */
	  this_thread::sleep_for(milliseconds(1000));
	}
      }
    }
  catch (const exception &e) {
    cerr << e.what() << endl;
    cerr << "Disconnected from server for unknown reason!" << endl;
    cerr << "The number of bugs found in this execution is " << bugs_amount << endl;
    cerr << "number" << "\t" <<  "err_no" << "\t" << "type of error" << endl;
    for (auto bug : bugs)
      cerr << bug.second << "\t" << bug.first.first << "\t" << bug.first.second << endl;
    ofstream outfile;
    outfile.open("bug_log.txt");
    outfile << "err_no" << "\t" << "err_info" << "\t" << "sql_info" << endl;
    for(auto error : sql_arr){
           for(auto sql_info : error.second){
                  outfile << error.first.first << "\t" << error.first.second << "\t";
                  outfile << sql_info << endl << endl;
           }
           outfile << endl << endl;
      }
    outfile.close();
    
    time_t end_time = time(0);
    int duration = static_cast<int>(end_time - start_time);
    cerr << "At the end of this test, the accumulated time is " << convert_time(duration) << endl;
    return 1;
  }
}
