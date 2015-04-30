// Copyright © 2015, The Network Protocol Company, Inc. All Rights Reserved.

#ifndef GAME_H
#define GAME_H

struct Input
{
    bool left;
    bool right;
    bool up;
    bool down;
    bool push;
    bool pull;

    Input()
    {
        left = false;
        right = false;
        up = false;
        down = false;
        push = false;
        pull = false;
    }

    bool operator == ( const Input & other )
    {
        return left == other.left    &&
              right == other.right   &&
                 up == other.up      &&
               down == other.down    &&
               push == other.push    &&
               pull == other.pull;
    }
    
    bool operator != ( const Input & other )
    {
        return ! ( (*this) == other );
    }
};

extern void game_process_player_input( struct World & world, const Input & input, int player_id );

#endif // #ifndef GAME_H
