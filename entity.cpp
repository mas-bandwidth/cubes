// Copyright © 2015, The Network Protocol Company, Inc. All Rights Reserved.

#include "entity.h"

// todo: authority update should be a pass that is performed at the entity (base entity) level
// even though it is primarily driven by physics initially, the idea that a client may need to
// be able to take authority over and predict a non-physically simulated object means this must
// live at the entity level, not physics mgr level.

/*
void UpdateAuthority( float deltaTime )
{
    // update authority timeout + force authority for any active player cubes
    
    int maxActiveId = 0;
    for ( int i = 0; i < activeObjects.GetCount(); ++i )
    {
        ActiveObject & activeObject = activeObjects.GetObject( i );
        if ( activeObject.id >= 1 && activeObject.id <= (uint32_t) MaxPlayers )
        {
            activeObject.authority = activeObject.id - 1;
            activeObject.authorityTime = 0;
        }
        else if ( activeObject.enabled )
        {
            activeObject.authorityTime = 0;
        }
        else
        {
            activeObject.authorityTime++;
            if ( activeObject.authorityTime > 100 )
                activeObject.authority = MaxPlayers;
        }
        if ( activeObject.activeId > (ActiveId) maxActiveId )
            maxActiveId = activeObject.activeId;
    }

    // build map from active id to active index
    
    std::vector<int> activeIdToIndex( maxActiveId + 1 );
    for ( int i = 0; i < activeObjects.GetCount(); ++i )
    {
        ActiveObject & activeObject = activeObjects.GetObject( i );
        activeIdToIndex[activeObject.activeId] = i;
    }
    
    // interaction based authority for active objects

    if ( InGame() && !GetFlag( FLAG_DisableInteractionAuthority ) )
    {
        for ( int playerId = 0; playerId < MaxPlayers; ++playerId )
        {
            std::vector<bool> interacting( maxActiveId + 1, false );
            std::vector<bool> ignores( maxActiveId + 1, false );
            std::vector<int> queue( activeObjects.GetCount() );
            int head = 0;
            int tail = 0;

            for ( int i = 0; i < activeObjects.GetCount(); ++i )
            {
                ActiveObject & activeObject = activeObjects.GetObject( i );
                const int activeId = activeObject.activeId;
                assert( activeId >= 0 );
                assert( activeId < (int) ignores.size() );
                assert( activeId < (int) interacting.size() );
                if ( activeObject.authority == playerId )
                {
                    interacting[activeId] = true;
                    assert( head < activeObjects.GetCount() );
                    queue[head++] = activeObject.activeId;
                }
                else if ( activeObject.authority != MaxPlayers || activeObject.enabled == 0 )
                {
                    ignores[activeId] = true;
                }
            }
            
            while ( head != tail )
            {
                const std::vector<uint16_t> & objectInteractions = simulation->GetObjectInteractions( queue[tail] );
                for ( int i = 0; i < (int) objectInteractions.size(); ++i )
                {
                    const int activeId = objectInteractions[i];
                    assert( activeId >= 0 );
                    assert( activeId < (int) interacting.size() );
                    if ( !ignores[activeId] && !interacting[activeId] )
                    {
                        assert( head < activeObjects.GetCount() );
                        queue[head++] = activeId;
                    }
                    interacting[activeId] = true;
                }
                tail++;
            }
            
            for ( int i = 0; i <= maxActiveId; ++i )
            {
                if ( interacting[i] )
                {
                    int index = activeIdToIndex[i];
                    ActiveObject & activeObject = activeObjects.GetObject( index );
                    if ( activeObject.authority == MaxPlayers || activeObject.authority == playerId )
                    {
                        activeObject.authority = playerId;
                        if ( activeObject.enabled )
                            activeObject.authorityTime = 0;
                    }
                }
            }
        }
    }
}
*/
