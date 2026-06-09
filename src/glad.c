#include "glad/glad.h"

PFNGLGENVERTEXARRAYSPROC glad_glGenVertexArrays = NULL;
PFNGLBINDVERTEXARRAYPROC glad_glBindVertexArray = NULL;
PFNGLDELETEVERTEXARRAYSPROC glad_glDeleteVertexArrays = NULL;
PFNGLGENBUFFERSPROC glad_glGenBuffers = NULL;
PFNGLBINDBUFFERPROC glad_glBindBuffer = NULL;
PFNGLBUFFERDATAPROC glad_glBufferData = NULL;
PFNGLDELETEBUFFERSPROC glad_glDeleteBuffers = NULL;
PFNGLENABLEVERTEXATTRIBARRAYPROC glad_glEnableVertexAttribArray = NULL;
PFNGLVERTEXATTRIBPOINTERPROC glad_glVertexAttribPointer = NULL;
PFNGLCREATESHADERPROC glad_glCreateShader = NULL;
PFNGLSHADERSOURCEPROC glad_glShaderSource = NULL;
PFNGLCOMPILESHADERPROC glad_glCompileShader = NULL;
PFNGLGETSHADERIVPROC glad_glGetShaderiv = NULL;
PFNGLGETSHADERINFOLOGPROC glad_glGetShaderInfoLog = NULL;
PFNGLDELETESHADERPROC glad_glDeleteShader = NULL;
PFNGLCREATEPROGRAMPROC glad_glCreateProgram = NULL;
PFNGLATTACHSHADERPROC glad_glAttachShader = NULL;
PFNGLLINKPROGRAMPROC glad_glLinkProgram = NULL;
PFNGLGETPROGRAMIVPROC glad_glGetProgramiv = NULL;
PFNGLGETPROGRAMINFOLOGPROC glad_glGetProgramInfoLog = NULL;
PFNGLDELETEPROGRAMPROC glad_glDeleteProgram = NULL;
PFNGLUSEPROGRAMPROC glad_glUseProgram = NULL;
PFNGLGETUNIFORMLOCATIONPROC glad_glGetUniformLocation = NULL;
PFNGLUNIFORMMATRIX4FVPROC glad_glUniformMatrix4fv = NULL;
PFNGLUNIFORM2FVPROC glad_glUniform2fv = NULL;
PFNGLUNIFORM3FVPROC glad_glUniform3fv = NULL;
PFNGLUNIFORM1FPROC glad_glUniform1f = NULL;
PFNGLUNIFORM1IPROC glad_glUniform1i = NULL;

static void* load_required(GLADloadproc load, const char* name)
{
    return load(name);
}

int gladLoadGLLoader(GLADloadproc load)
{
    glad_glGenVertexArrays = (PFNGLGENVERTEXARRAYSPROC)load_required(load, "glGenVertexArrays");
    glad_glBindVertexArray = (PFNGLBINDVERTEXARRAYPROC)load_required(load, "glBindVertexArray");
    glad_glDeleteVertexArrays = (PFNGLDELETEVERTEXARRAYSPROC)load_required(load, "glDeleteVertexArrays");
    glad_glGenBuffers = (PFNGLGENBUFFERSPROC)load_required(load, "glGenBuffers");
    glad_glBindBuffer = (PFNGLBINDBUFFERPROC)load_required(load, "glBindBuffer");
    glad_glBufferData = (PFNGLBUFFERDATAPROC)load_required(load, "glBufferData");
    glad_glDeleteBuffers = (PFNGLDELETEBUFFERSPROC)load_required(load, "glDeleteBuffers");
    glad_glEnableVertexAttribArray = (PFNGLENABLEVERTEXATTRIBARRAYPROC)load_required(load, "glEnableVertexAttribArray");
    glad_glVertexAttribPointer = (PFNGLVERTEXATTRIBPOINTERPROC)load_required(load, "glVertexAttribPointer");
    glad_glCreateShader = (PFNGLCREATESHADERPROC)load_required(load, "glCreateShader");
    glad_glShaderSource = (PFNGLSHADERSOURCEPROC)load_required(load, "glShaderSource");
    glad_glCompileShader = (PFNGLCOMPILESHADERPROC)load_required(load, "glCompileShader");
    glad_glGetShaderiv = (PFNGLGETSHADERIVPROC)load_required(load, "glGetShaderiv");
    glad_glGetShaderInfoLog = (PFNGLGETSHADERINFOLOGPROC)load_required(load, "glGetShaderInfoLog");
    glad_glDeleteShader = (PFNGLDELETESHADERPROC)load_required(load, "glDeleteShader");
    glad_glCreateProgram = (PFNGLCREATEPROGRAMPROC)load_required(load, "glCreateProgram");
    glad_glAttachShader = (PFNGLATTACHSHADERPROC)load_required(load, "glAttachShader");
    glad_glLinkProgram = (PFNGLLINKPROGRAMPROC)load_required(load, "glLinkProgram");
    glad_glGetProgramiv = (PFNGLGETPROGRAMIVPROC)load_required(load, "glGetProgramiv");
    glad_glGetProgramInfoLog = (PFNGLGETPROGRAMINFOLOGPROC)load_required(load, "glGetProgramInfoLog");
    glad_glDeleteProgram = (PFNGLDELETEPROGRAMPROC)load_required(load, "glDeleteProgram");
    glad_glUseProgram = (PFNGLUSEPROGRAMPROC)load_required(load, "glUseProgram");
    glad_glGetUniformLocation = (PFNGLGETUNIFORMLOCATIONPROC)load_required(load, "glGetUniformLocation");
    glad_glUniformMatrix4fv = (PFNGLUNIFORMMATRIX4FVPROC)load_required(load, "glUniformMatrix4fv");
    glad_glUniform2fv = (PFNGLUNIFORM2FVPROC)load_required(load, "glUniform2fv");
    glad_glUniform3fv = (PFNGLUNIFORM3FVPROC)load_required(load, "glUniform3fv");
    glad_glUniform1f = (PFNGLUNIFORM1FPROC)load_required(load, "glUniform1f");
    glad_glUniform1i = (PFNGLUNIFORM1IPROC)load_required(load, "glUniform1i");

    return glad_glGenVertexArrays && glad_glBindVertexArray && glad_glGenBuffers &&
           glad_glBindBuffer && glad_glBufferData && glad_glCreateShader &&
           glad_glCreateProgram && glad_glUseProgram && glad_glUniformMatrix4fv;
}
