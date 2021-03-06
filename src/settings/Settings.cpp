#include <fstream>
#include <iostream>
#include <stdexcept>
#include <map>
#include <vector>
#include <ctime>
#include <sys/wait.h>
#include <dirent.h>
#include <sstream>
#include <unistd.h>
#include <string.h>
#include <stdio.h>
#include <stdlib.h>
#include <future>

#include "Settings.h"

using namespace std;

Settings::Settings(): wget(nullptr) {
	Parse(get_working_path() + "settings.conf");
	user_home = "/home/" + GetUserName();
	system_theme = CommandOutput(Key("system_theme"));
	local_config = user_home + "/" + Key("ucfg_path");
}

// Source: http://stackoverflow.com/questions/6892754/creating-a-simple-configuration-file-and-parser-in-c
void Settings::Parse(string file_path) {
	ifstream file(file_path.c_str());
	string id, eq, val, line;
	while(getline(file, line)) {
		istringstream iss(line);
		if(iss >> id >> eq >> val) {
			if (id[0] == '#') {
				continue;  // skip comments
			}
			if (eq != "=") {
				cerr << "[ERROR] " << "Mismatch in the file." << endl;
				continue;
			}
			
			char on_line;
			while(iss.get(on_line))
				val += on_line;
			
			options[id] = val;
		}
	}	
	file.close();
}

string Settings::Key(string key) const {
	try {
		map<string, string>::const_iterator it = options.find(key);
		if (it != options.end()) {
			return it->second;
		}
		else {
			cout << "It looks like \"settings.conf\" file is missing. Please redownload Steam Skin Manager." << endl;
			throw runtime_error("Key not found.");
		}
	}
	catch(...) {
		throw runtime_error("An exception occured during the " + key + " key-search.");
	}
}

int Settings::StringToInt(string value) {
	istringstream num_stream(value);
	int i;
	num_stream >> i;
	return i;
}

string Settings::GetTip(bool up) {
	if(!tips.download) {		
		try {
			wget = new future<void>(async(launch::async, [&] {
				tips.download = true;
				tips.tip.push_back(CommandOutput("echo \"$(wget iubuntu.cz/Steam/variable_content/tip.php?lang=" + string(locale("").name()) + " -q -O -)\""));
				tips.size++;	
				return;
			}));
		}
		catch(...) {
			cerr << "Can't load remote tip from network." << endl;
		}
	}
	
	if(up) {
		tips.current++;
		if(tips.current > tips.size - 1) {
			tips.current = 0;
		}
	}
	else {
		tips.current--;
		if(tips.current > tips.size - 1) {
			tips.current = tips.size - 1;
		}
	}
		
	return tips.tip[tips.current];
}

void Settings::TrimLine(string & str) const {
	while(!str.empty() && str.back() == '\n') {
		str.erase(str.length()-1);
	}
}

string Settings::CommandOutput(string cmd) const {
	string data;
	FILE * stream;
	const int max_buffer = 256;
	char buffer[max_buffer];
		
	stream = popen(cmd.c_str(), "r");
	if(stream) {
		while(!feof(stream)) {
			if(fgets(buffer, max_buffer, stream) != nullptr) {
				data.append(buffer);
			}
		}
		int ret = pclose(stream)/256;
		if(ret != 0)
			cerr << "Command could not be executed, error code: " << ret << endl;
	}
	
	TrimLine(data);
	return data;
}

string Settings::GetFileContent(string file) const {
	string lines = "";
	try {
		ifstream input(file.c_str());
		if (input.is_open()) {
			for(string line; getline(input, line); ) {
				if(line != "")
					lines += line + "\n";
			}
			input.close();
		}
		else
			cerr << "Can't open " << file << " file." << endl;
	}
	catch(...) {
		cerr << "An exception was thrown due to a problem reading a file" << endl;
	}
	
	TrimLine(lines);
	return lines;
}

string Settings::GetSoftwareVersion() const {
	return Key("version");
}

string Settings::GetApplicationName() const {
	return Key("app_name");
}

vector<string> Settings::GetListOfAvaialbleFolders(string folder) const {
	vector<string> list;
	DIR *dir;
	struct dirent *entry;
	
	if(folder != "") {
		try {
			dir = opendir(folder.c_str());
			if (dir != NULL) {
				while ((entry = readdir(dir)) != NULL) {
					if (entry->d_type == DT_DIR && (entry->d_name != string(".") && entry->d_name != string("..")))
						list.push_back(entry->d_name);
				}
			}
		}
		catch(...) {
			return list;
		}
	}
	
	return list;
}

string Settings::GetSystemTheme() const {
	return system_theme;
}

string Settings::GetPath(string type) const {
	if(type == "theme")
		return Key("themes_path");
	
	return NULL;
}

string Settings::GetLocalPath() const
{
	return user_home + "/" + Key("ucfg_path");
}

string Settings::GetHomePath() const {
	return user_home;
}

string Settings::GetSystemPath() const {
	return get_working_path() + Key("data_path");
}

string Settings::TimePartCorrector(int stamp) {
	if(stamp < 10)
		return ("0" + to_string(stamp));
	else
		return to_string(stamp);
}

void Settings::UpdateAccessTimestamp() {
	CommandOutput("mkdir -p \"" + local_config + "\"");
	
	try {
		ofstream cfg;
		cfg.open((local_config + "last_access").c_str());
		time_t t = time(0);
		struct tm * now = localtime( & t );
		cfg << now->tm_mday << "." << (now->tm_mon + 1) << "." << (now->tm_year + 1900) << "_";
		cfg << TimePartCorrector(now->tm_hour) << ":" << TimePartCorrector(now->tm_min) << ":" << TimePartCorrector(now->tm_sec) << endl;
		cfg.close();
	}
	catch (...) {
		cerr << "Can't write to a file.";
	}
}

bool Settings::FileExists(string filename) const {
	ifstream f(filename);
	return f;
}

string Settings::GetLastTime() {
	if(FileExists(local_config + "last_access")) {
		return GetFileContent(local_config + "last_access");
	}
	else {
		return _("never ran before");
	}
}

bool Settings::SetSkin(string path) {
	string local_command = get_working_path() + Key("steamskinapplier") + " -i \"" + path + "\"";
	cout << "Executing: " << local_command << endl;
	int status_code = system(local_command.c_str());
    if(status_code == 0)
		return true;
	return false;
}

bool Settings::SetInstalledSkin(string local_skin_path) {
	string local_command = get_working_path() + Key("steamskinapplier") + " -i \"" + get_working_path() + local_skin_path + "\"";
	cout << "Executing: " << local_command << endl;
	int status_code = system(local_command.c_str());
	if(status_code == 0)
		return true;
	return false;
}

bool Settings::RevertSkin() {
	string local_command = get_working_path() + Key("steamskinapplier") + " -r";
	cout << "Executing: " << local_command << endl;
	int status_code = system(local_command.c_str());
	if(status_code == 0)
		return true;
	return false;
}

string Settings::GetCurrentTheme() const {	
	return CommandOutput(get_working_path() + Key("steamskinapplier") + " -g");
}

string Settings::get_working_path() const {
	char temp[256];
	readlink("/proc/self/exe", temp, 256);
	
	string bin_path = string(temp);
	size_t found = bin_path.find_last_of("/\\");
	bin_path = bin_path.substr(0, found + 1);
	
	return bin_path;
	
	/* 	return(getcwd(temp, 256) ? (string(temp) + "/") : string("")); */
}

string Settings::GetUserName() {
	return string(getenv("USER"));
}

string Settings::GetFullUserName() {
	string name = CommandOutput("getent passwd " + GetUserName() + " | cut -d ':' -f 5 | cut -d ',' -f 1");
	if(name != "") {
		return name;
	}
	return GetUserName();
}

bool Settings::CreateLauncher() {
	int status_code = system((get_working_path() + Key("launchergenerator")).c_str());
	if(status_code == 0)
		return true;
	return false;
}

Settings::~Settings() {
	delete wget;
}
