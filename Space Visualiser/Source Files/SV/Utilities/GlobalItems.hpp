////
////Include the precompiled global items
////
//#include"SV/Utilities/Precompiled Global.hpp"
#pragma once


//Define header constant, to allow for inclusion only once
#define GlobalItems


//
//Define byte type
//
typedef unsigned char byte;

//
//Include OpenGL binding
//
class GLObject;
#include"SV/OpenGL/OpenGL Context.hpp"
using glm::uint;

//
//Global Space Visualiser classes to include
//
#include"SV/Scene/Transformation.hpp"

//
//Constants
//
#define FINAL_DISPLAY_WIDTH 3840
#define FINAL_DISPLAY_HEIGHT 2160
//