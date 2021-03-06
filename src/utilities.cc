#include <iostream>
#include <fstream>
#include <string>
#include <string.h>
#include <stdio.h>

#include <sys/types.h>
#include <sys/stat.h>
#include <limits.h>

#include "git2.h"

#ifndef _WIN32

#include <unistd.h>

static void cget_create_dir(const std::string &dir) {
	mkdir(dir.c_str(), S_IRWXU | S_IRWXG | S_IROTH | S_IXOTH);
}

#else
#ifndef PATH_MAX
#define PATH_MAX 4096
#endif
#include <direct.h>
#include <windows.h>
static void cget_create_dir(const std::string& dir) {
  CreateDirectory(dir.c_str(), NULL);
}
#endif

#include "generated/cget_core_cmake.h"
#include <cget/utilities.h>

const char *packageFile = "./package.cmake";
std::string corePath = ".cget/core.cmake";

static bool file_exists(const std::string &f) {
	std::ifstream file(f);
	return file.good();
}

static void copy(const char *a, const char *b) {
	std::fstream infile(a);
	std::ofstream outfile(b);

	std::string line;
	while (std::getline(infile, line)) {
		outfile << line << std::endl;
	}
}

namespace cget {
  std::string RepoMetadata::FullUrl() const {
    switch (source) {
    case RepoSource::GITHUB:
    case RepoSource::REGISTRY:      
      return "https://github.com/" + url + ".git"; 
    default:
      return url;
    }
  }
  
	void insert_hook() {
		const char *original = "./CMakeLists.txt";
		const char *temp = "./CMakeLists.txt.tmp";
		std::string insertHook("include(package.cmake)");

		if (file_exists(original)) {

			std::fstream infile(original, std::ios_base::in | std::ios_base::out);
			std::ofstream outfile(temp);


			std::string line;
			bool hasInsertHook = false;
			const char *project = "project";

			while (std::getline(infile, line) && hasInsertHook == false) {
				if (line == insertHook) {
					hasInsertHook = true;
					break;
				}

				outfile << line << std::endl;

				if (line.compare(0, strlen(project), project) == 0) {
					outfile << insertHook << std::endl;
				}
			}
			if (hasInsertHook == false) {
				copy(temp, original);
			}
			remove(temp);
			outfile.flush();
		} else {
			std::ofstream outfile(original);
			char cpath[PATH_MAX];

			getcwd(cpath, PATH_MAX - 1);
			std::string path(cpath);
			int from = path.find_last_of('/');
			if (from != std::string::npos)
				path = path.substr(from + 1);

			outfile << "cmake_minimum_required (VERSION 3.0)" << std::endl;
			outfile << "project(" << path << " C CXX)" << std::endl;
			outfile << std::endl;
			outfile << insertHook << std::endl;
			outfile << std::endl;
		}
	}


  void handle_libgit_ret(int ret) {
    if(ret != 0) {
      std::cerr << "Error with git: '" << ret << "' " << giterr_last()->message << std::endl;
    }
  }

  
  static int just_return_origin(git_remote **out, git_repository *repo, const char *name, const char *url, void *payload)
  {
    return git_remote_lookup(out, repo, name);
  }

  static int just_return_repo(git_repository **out, const char *path, int bare, void *payload)
  {
    git_submodule *sm = (git_submodule*)payload;
    return git_submodule_open(out, sm);
  }

  void submodule_add_if_not_exist(const std::string& url,
				  const std::string& path) {
    bool fileExists = file_exists(path + "/.git");

    if (!fileExists) {
      std::cout << "Adding submodule " << url << " at " << path << std::endl;
      git_submodule *sm = 0;
      git_repository *smrepo = 0, *g_repo = 0;

      handle_libgit_ret(git_repository_open(&g_repo, "."));
      std::cerr << git_repository_path(g_repo) << std::endl;
		
	        
      handle_libgit_ret(git_submodule_add_setup(&sm, g_repo,
						url.c_str(),
						path.c_str(),
						1));

      if(sm) {
	std::cerr << "Added" << std::endl;
	handle_libgit_ret(git_submodule_open(&smrepo, sm));

	git_remote* remote = 0;
	git_remote_lookup(&remote, smrepo, "origin");
	git_fetch_options fopts = GIT_FETCH_OPTIONS_INIT;
	handle_libgit_ret(git_remote_fetch(remote, NULL, &fopts, NULL));

	git_oid oid;
	handle_libgit_ret(git_reference_name_to_id(&oid, smrepo, "refs/remotes/origin/master"));

	git_object* branch = 0;
	handle_libgit_ret(git_object_lookup(&branch, smrepo, &oid, GIT_OBJ_ANY));
	handle_libgit_ret(git_reset(smrepo, branch, GIT_RESET_HARD, NULL));
	handle_libgit_ret(git_submodule_add_finalize(sm));
	git_submodule_free(sm);
	git_repository_free(smrepo);
      }

      git_repository_free(g_repo);
    }    
  }
  
  void init_project() {	
    submodule_add_if_not_exist("https://github.com/cget/cget-core", ".cget");
    if (file_exists(packageFile))
      return;
    std::ofstream cget(packageFile);
    cget << "include(${CGET_CORE_DIR}" << corePath << ")" << std::endl << std::endl;
  }


  void insert(const RepoMetadata &meta, const std::string& subrepo_loc) {
		init_project();
		std::fstream infile(packageFile, std::ios_base::in | std::ios_base::out);
		std::string line;
		bool hasInsert = false;
		std::string name = meta.name;

		auto suffix = name.find(".cget");
		if (suffix != std::string::npos)
			name = name.substr(0, suffix);

		std::string targetPrefix = "CGET_HAS_DEPENDENCY(" + name + " ";
		while (std::getline(infile, line) && hasInsert == false) {
			size_t pos = line.find(targetPrefix);
			size_t comment_pos = line.find("#");

			hasInsert |= (pos != std::string::npos) && comment_pos > pos;
		}
		infile.close();
		if (hasInsert) {
			std::cout << "ERROR: package.cmake already contains an entry for " << name << std::endl;
			return;
		}

		std::fstream outfile("./package.cmake", std::ios_base::app | std::ios_base::out);

		outfile << targetPrefix;

		if( (meta.source == RepoSource::GIT || meta.source == RepoSource::GITHUB || meta.source == RepoSource::REGISTRY) &&
		    !subrepo_loc.empty()) {
		  submodule_add_if_not_exist(meta.FullUrl(), subrepo_loc);
		  outfile << "SUBMODULE " << subrepo_loc << " ";
		} else {
		  outfile << RepoSource::ToString(meta.source) << " ";
		  switch (meta.source) {
		  case RepoSource::REGISTRY:
		    break;
		  default:
		    outfile << meta.url << " ";
		  }
		}
		
		if (meta.version.size())
			outfile << "VERSION " << meta.version;
		outfile << ")" << std::endl;


		switch (meta.source) {
			case RepoSource::GITHUB:
				std::cout << "Adding dependency to github repo '" << meta.url << "'." << std::endl;
				break;
			case RepoSource::GIT:
				std::cout << "Adding dependency to git repo at '" << meta.url << "'." << std::endl;
				break;
			case RepoSource::HG:
				std::cout << "Adding dependency to mercurial repo at '" << meta.url << "'." << std::endl;
				break;
			case RepoSource::SVN:
				std::cout << "Adding dependency to subversion repo at '" << meta.url << "'." << std::endl;
				break;
			case RepoSource::URL:
				std::cout << "Adding dependency to tarball at '" << meta.url << "'." << std::endl;
				break;
			case RepoSource::REGISTRY:
				std::cout << "Package found in cget registry as '" << meta.name << "'." << std::endl;
				break;
		}
	}

	std::string RepoSource::ToString(RepoSource::t t) {
		switch (t) {
			case RepoSource::UNKNOWN:
				return "UNKNOWN";
			case RepoSource::GITHUB:
				return "GITHUB";
			case RepoSource::GIT:
				return "GIT";
			case RepoSource::HG:
				return "HG";
			case RepoSource::SVN:
				return "SVN";
			case RepoSource::URL:
				return "URL";
			case RepoSource::REGISTRY:
				return "REGISTRY";
		}
		return "<UNKNOWN>";
	}

	RepoMetadata getMetaData(const std::string &id) {
		std::string name = id;
		auto pos = id.find_last_of("/");
		if (pos != std::string::npos)
			name = id.substr(pos + 1);
		RepoMetadata repo;
		repo.name = name;
		repo.source = RepoSource::GITHUB;
		repo.url = id;
		repo.version = "HEAD";
		return repo;
	};

}
