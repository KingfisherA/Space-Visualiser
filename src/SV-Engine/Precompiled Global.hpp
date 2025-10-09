#pragma once
//
#define SV_PRECOMPILED_HEADER

//
//Standard Library
//

//Memory
#include<memory>

//Algorithms
#include<algorithm>

//Functions
#include<functional>

//Data structures
#include<vector>
#include<stack>
#include<list>
#include<map>
#include<unordered_map>
#include<array>

//Strings and streams
#include<sstream>
#include<string>
#include<string_view>

//Error Handling
#include<exception>

//IO
#include <iostream>
#include <fstream>

//Threading
#include<thread>
#include<mutex>
#include<shared_mutex>

//Time
#include<chrono>

#ifdef _WIN32
//
//Standard Windows Library
//
#include <windows.h>

//Text Mapping
#include<tchar.h>

//Console input/output
#include <conio.h>

//#include <winuser.h>
#endif
