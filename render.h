// Copyright © 2015, The Network Protocol Company, Inc. All Rights Reserved.

#ifndef RENDER_H
#define RENDER_H

#include "core.h"
#include <stdio.h>
#include <stdlib.h>
#include <GL/glew.h>
#include <GLFW/glfw3.h>

inline void clear_opengl_error()
{
    while ( glGetError() != GL_NO_ERROR );
}

inline void check_opengl_error( const char * message )
{
    int error = glGetError();
    if ( error != GL_NO_ERROR )
    {
        printf( "opengl error: %s (%s)\n", gluErrorString( error ), message );
        exit( 1 );
    }    
}

uint32_t load_shader( const char * vertex_file_path, const char * fragment_file_path );

void delete_shader( uint32_t shader );

#endif // #ifndef RENDER_H
