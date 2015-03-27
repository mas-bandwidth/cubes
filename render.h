// Copyright © 2015, The Network Protocol Company, Inc. All Rights Reserved.

#ifndef RENDER_H
#define RENDER_H

#include <GL/glew.h>
#include <GLFW/glfw3.h>

void clear_opengl_error()
{
    while ( glGetError() != GL_NO_ERROR );
}

void check_opengl_error( const char * message )
{
    int error = glGetError();
    if ( error != GL_NO_ERROR )
    {
        printf( "opengl error: %s (%s)\n", gluErrorString( error ), message );
        exit( 1 );
    }    
}

#endif // #ifndef RENDER_H