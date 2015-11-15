#include <iostream>
#include <fstream>
#include <string>
#include <string.h>
#include <stdio.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <limits.h>
#include <unistd.h>

#include "generated/cget_core_cmake.h"
#include <cget/utilities.h>

const char * packageFile = "./package.cmake";
std::string corePath = ".cget/core.cmake";

static bool file_exists(const std::string& f) {
  std::ifstream file(f);
  return file.good();
}

static void copy(const char* a, const char* b) {
  std::fstream infile(a);
  std::ofstream outfile(b);

  std::string line;
  while (std::getline(infile, line)) {
    outfile << line << std::endl;
  }
}

namespace cget {

  void insert_hook() {
    const char* original = "./CMakeLists.txt";
    const char* temp     = "./CMakeLists.txt.tmp";
    std::string insertHook("include(package.cmake)");
    
    if(file_exists(original)) {

      std::fstream infile(original, std::ios_base::in | std::ios_base::out);
      std::ofstream outfile(temp);

  
      std::string line;
      bool hasInsertHook = false; 
      const char* project = "project"; 

      while (std::getline(infile, line) && hasInsertHook == false) {
	if(line == insertHook) {
	  hasInsertHook = true;
	  break;
	}
    
	outfile << line << std::endl;

	if(line.compare(0, strlen(project), project) == 0) {
	  outfile << insertHook << std::endl;
	}   
      }
      if(hasInsertHook == false) {
	copy(temp, original);
      }
      remove(temp);
      outfile.flush();
    } else {
      std::ofstream outfile(original);
      char cpath[PATH_MAX];

      getcwd(cpath,PATH_MAX-1);
      std::string path(cpath);
      int from = path.find_last_of('/');
      if(from != std::string::npos)
	path = path.substr(from+1);

      outfile << "cmake_minimum_required (VERSION 3.0)" << std::endl;
      outfile << "project(" << path << " C CXX)" << std::endl;
      outfile << std::endl;
      outfile << insertHook << std::endl;
      outfile << std::endl;
    }
  }

  void init_project() {  
    if(!file_exists(corePath)) {
      mkdir(".cget", S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
      std::ofstream corelib(corePath);
      std::cout << "IMPORTANT: Default core library file added at '.cget/core.cmake'. It's recommended that you add it to your repo. " << std::endl;
      corelib.write((const char*)_cget_core_cmake, _cget_core_cmake_len);
    }

    if(file_exists(packageFile))
      return;
    std::ofstream cget(packageFile);     
    cget << "include(" << corePath << ")" << std::endl << std::endl;
  }


  void insert(const RepoMetadata& meta) {
    init_project(); 
    std::fstream infile(packageFile, std::ios_base::in | std::ios_base::out);
    std::string line;
    bool hasInsert = false;
    std::string name = meta.name;

    auto suffix = name.find(".cget");
    if(suffix != std::string::npos)
      name = name.substr(0, suffix); 
    
    std::string targetPrefix = "CGET_HAS_DEPENDENCY(" + name + " ";
    while (std::getline(infile, line) && hasInsert == false) {
      size_t pos = line.find(targetPrefix);
      size_t comment_pos = line.find("#"); 
      
      hasInsert |= (pos != std::string::npos) && comment_pos > pos;
    }
    infile.close();
    if(hasInsert) {
      std::cout << "ERROR: package.cmake already contains an entry for " << name << std::endl;
      return;
    }
  
    std::fstream outfile("./package.cmake", std::ios_base::app | std::ios_base::out);
    
    outfile << targetPrefix << RepoSource::ToString(meta.source) << " ";
    switch(meta.source) {
      case RepoSource::REGISTRY:
      break;
    default:
      outfile << meta.url << " ";
    }

    if(meta.version.size())
      outfile << "VERSION " << meta.version;
    outfile << ")" << std::endl;

  }

  std::string RepoSource::ToString(RepoSource::t t) {
    switch(t) {
    case RepoSource::UNKNOWN: return "UNKNOWN";
    case RepoSource::GITHUB: return "GITHUB";
    case RepoSource::GIT: return "GIT";
    case RepoSource::HG: return "HG";
    case RepoSource::SVN: return "SVN";
    case RepoSource::URL: return "URL";
    case RepoSource::REGISTRY: return "REGISTRY";
    }
  }

  RepoMetadata getMetaData(const std::string& id) {
    std::string name = id;
    auto pos = id.find_last_of("/");
    if(pos != std::string::npos)
      name = id.substr(pos+1); 
    return (RepoMetadata) {
      name, RepoSource::GITHUB, id, "HEAD"
	};
  };

}