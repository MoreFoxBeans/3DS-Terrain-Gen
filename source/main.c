#include <stdio.h>
#include <stdlib.h>
#include <3ds.h>
#include <citro2d.h>
#include "perlin.h"

/*************************** MACROS ***************************/

#define color(r, g, b, a)   ((r) | ((g) << (u32)8) | ((b) << (u32)16) | ((a) << (u32)24))
#define gray(v, a)          ((v) | ((v) << (u32)8) | ((v) << (u32)16) | ((a) << (u32)24))
#define alpha(a)            ((a) << (u32)24)

#define map(v, a, b, c, d)  ((c) + (((d) - (c)) / ((b) - (a))) * ((v) - (a)))

/*************************** SCREEN ***************************/

#define TOP_WIDTH		400
#define BOTTOM_WIDTH	320
#define LCD_HEIGHT		240

/*************************** COLORS ***************************/

#define BLACK           color( 0, 0, 0, 255 )
#define WHITE           color( 255, 255, 255, 255 )

#define FOG_RAIN        color( 46, 53, 65, 178 )
#define RAIN_START      color( 201, 212, 226, 16 )
#define RAIN_END        color( 201, 212, 226, 64 )

#define FOG_DAY         color( 134, 196, 219, 32 )
#define FOG_NIGHT       color( 28, 53, 72, 64 )

#define HEADER_TOP      color( 0, 0, 0, 128 )
#define HEADER_BOTTOM   color( 0, 0, 0, 0 )
#define HEADER_TEXT     color( 255, 255, 255, 240 )

#define GLOW_DAY        color( 255, 223, 185, 96 )
#define TRANS_DAY       color( 255, 223, 185, 0 )

#define GLOW_NIGHT      color( 174, 204, 227, 96 )
#define TRANS_NIGHT     color( 174, 204, 227, 0 )

#define SHADOW          color( 40, 26, 47, 160 )
#define TRANS_SHADOW    color( 40, 26, 47, 0 )

#define GROUND_TOP      color( 97, 60, 42, 255 )
#define GROUND_BOTTOM   color( 36, 20, 12, 255 )
#define GROUND_DOT1     color( 192, 192, 192, 64 )
#define GROUND_DOT2     color( 0, 0, 0, 64 )

#define GRASS_TIP       color( 116, 184, 7, 255 )
#define GRASS_BASE      color( 66, 138, 3, 255 )

/*************************** PARAMS ***************************/

#define GRASS_HEIGHT    8

/*********************** LAME VARIABLES ***********************/

circlePosition circlePad;
C2D_Sprite day;
C2D_Sprite night;
C2D_TextBuf g_staticBuf;
C2D_Text g_staticText;
C2D_Font font;

/*********************** COOL VARIABLES ***********************/

u8 height[TOP_WIDTH];
u32 scroll = 1 << 31;
bool dayTime = true;
float raining = 0.0;

/************************* FUNCTIONS **************************/

/**
 * @brief Generate the height of a location
 * 
 * @param x The location to generate
 * @return u8 Height
 */
u8 noise( u32 x )
{
    double n = pnoise1d( x * 0.005, 1, 1, 12345 ) /  2.0
             + pnoise1d( x *  0.04, 1, 1, 12345 ) /  8.0
             + pnoise1d( x *   0.1, 1, 1, 12345 ) / 32.0; // DIY octaves
    u8 h = map(n, -1.0, 1.0, 0, LCD_HEIGHT);
    return h;
}

/**
 * @brief Move the height buffer right
 * 
 */
void scrollLeft( u8 amount )
{
    scroll -= amount;
    memmove( &height[amount], &height[0], sizeof(u8) * (TOP_WIDTH - amount) ); // move right 1
    for (u8 i = 0; i < amount; ++i) { // populate blank space
        height[i] = noise( scroll + i );
    }
}

/**
 * @brief Move the height buffer left
 * 
 */
void scrollRight( u8 amount )
{
    scroll += amount;
    memmove( &height[0], &height[amount], sizeof(u8) * (TOP_WIDTH - amount) ); // move left 1
    for (u16 i = TOP_WIDTH - amount; i < TOP_WIDTH; ++i) { // populate blank space
        height[i] = noise( scroll + i );
    }
}

/**
 * @brief Generate a new screen
 * 
 */
void generate( void )
{
    for ( u16 i = 0; i < TOP_WIDTH; ++i ) height[i] = noise( scroll + i );
}

/**
 * @brief Set the header text
 * 
 * @param str String to set it to
 */
void setText( const char *str )
{
    C2D_TextBufClear( g_staticBuf );
    C2D_TextFontParseLine( &g_staticText, font, g_staticBuf, str, 0 );
    C2D_TextOptimize( &g_staticText );
}

/**
 * @brief Rain Check!
 * 
 */
void rainCheck( void )
{
    raining = fmax( pnoise1d( scroll * 0.002, 1, 1, 12345 ) * 2.0 - 1.0, 0.0 ); // maybe...
}

/**
 * @brief Render the rain effect
 * 
 */
void drawRain( void )
{
    C2D_DrawRectSolid( 0, 0, 0, TOP_WIDTH, LCD_HEIGHT, FOG_RAIN - alpha( 178 - (u8)(raining * 178.0) ) );
    u16 xofs = osGetTime() >> 2;
    u16 yofs = osGetTime() >> 1;

    u32 rain_1 = RAIN_START - alpha( 16 - (u8)(raining * 16.0) );
    u32 rain_2 = RAIN_END - alpha( 64 - (u8)(raining * 64.0) );

    for ( u8 i = 0; i < 255; ++i ) // why from 0 to 254? i have no idea!
    {
        s16 x = (s32)(map( rawnoise( i ), -1.0, 1.0, xofs, TOP_WIDTH + xofs )) % TOP_WIDTH - 6; // meth
        s16 y = (s32)(map( rawnoise( i + 256 ), -1.0, 1.0, yofs, LCD_HEIGHT + yofs )) % LCD_HEIGHT - 24;
        C2D_DrawLine( x, y, rain_1, x + 12, y + 24, rain_2, rawnoise( (u16)i << 3 ) + 1, 0 );
    }
}

/**
 * @brief Render the terrain
 * 
 */
void drawTerrain( void )
{
    for ( u16 i = 0; i < TOP_WIDTH; ++i )
    {
        u8 h = height[i];
        u8 y = LCD_HEIGHT - h;
        u8 v = (u8)((rawnoise(scroll + i) + 1) * 4.0);
        
        C2D_DrawLine( i, y, GROUND_TOP + gray(v, 255), i, LCD_HEIGHT, GROUND_BOTTOM - gray(v, 0), 1, 0 ); // dirt
        C2D_DrawLine( i, y + 1, GRASS_BASE,
                      i + round( pnoise1d( osGetTime() / 1000.0 + (i + scroll) / 50.0 + rawnoise( i + scroll ) / 4.0, 1, 1, 12345 ) * 6.0 ) + 2,
                      y - GRASS_HEIGHT - round( rawnoise( i + scroll ) - 1.0 ),
                      GRASS_TIP, 1, 0 ); // grass

        if (((scroll + i) % 2) == 0)
        {
            u8 y;

            y = (rawnoise(scroll + i) + 1) / 2 * height[i];
            C2D_DrawRectSolid( i, LCD_HEIGHT - 1 - y, 0, 1, 1, GROUND_DOT1 ); // light dots
            
            y = (rawnoise(scroll + i + 256) + 1) / 2 * height[i];
            C2D_DrawRectSolid( i, LCD_HEIGHT - 1 - y, 0, 1, 1, GROUND_DOT2 ); // dark dots
        }
    }
}

/**
 * @brief Render the top screen
 * 
 */
void renderTop( void )
{
    // setText( "The quick brown fox jumps over the lazy dogThe quick brown fox jumps over the lazy dogThe quick brown fox jumps over the lazy dogThe quick brown fox jumps over the lazy dogThe quick brown fox jumps over the lazy dogThe quick brown fox jumps over the lazy dogThe quick brown fox jumps over the lazy dogThe quick brown fox jumps over the lazy dogThe quick brown fox jumps over the lazy dogThe quick brown fox jumps over the lazy dogThe quick brown fox jumps over the lazy dogThe quick brown fox jumps over the lazy dogThe quick brown fox jumps over the lazy dogThe quick brown fox jumps over the lazy dogThe quick brown fox jumps over the lazy dogThe quick brown fox jumps over the lazy dogThe quick brown fox jumps over the lazy dogThe quick brown fox jumps over the lazy dog" );

    if ( dayTime ) C2D_DrawSprite( &day );
    else C2D_DrawSprite( &night );

    drawRain();
    drawTerrain();

    C2D_DrawRectangle( 0, LCD_HEIGHT / 2, 0, TOP_WIDTH, LCD_HEIGHT / 2, TRANS_SHADOW, TRANS_SHADOW, SHADOW, SHADOW ); // Purple shadow on bottom

    C2D_DrawRectSolid( 0, 0, 0, TOP_WIDTH, LCD_HEIGHT, FOG_RAIN - alpha( 178 - (u8)(raining * 178.0) ) );

    u32 sub = alpha( (u8)(raining * 96.0) );

    // Top gradient and fog overlay
    if ( dayTime )
    {
        C2D_DrawRectangle( 0, 0, 0, TOP_WIDTH, LCD_HEIGHT / 2, GLOW_DAY - sub, GLOW_DAY - sub, TRANS_DAY, TRANS_DAY );
        C2D_DrawRectSolid( 0, 0, 0, TOP_WIDTH, LCD_HEIGHT, FOG_DAY - alpha( (u8)(raining * 32.0) ) );
    }
    else
    {
        C2D_DrawRectangle( 0, 0, 0, TOP_WIDTH, LCD_HEIGHT / 2, GLOW_NIGHT - sub, GLOW_NIGHT - sub, TRANS_NIGHT, TRANS_NIGHT );
        C2D_DrawRectSolid( 0, 0, 0, TOP_WIDTH, LCD_HEIGHT, FOG_NIGHT - alpha( (u8)(raining * 64.0) ) );
    }

    // C2D_DrawRectangle( 0, 0, 0, TOP_WIDTH, 32, HEADER_TOP, HEADER_TOP, HEADER_BOTTOM, HEADER_BOTTOM );
    // C2D_DrawText( &g_staticText, C2D_WithColor, 0.0f, 0.0f, 0.0f, 1.0f, 1.0f, HEADER_TEXT );
}

/**
 * @brief Render the bottom screen
 * 
 */
void renderBottom( void )
{

}

/**
 * @brief Free up resources on exit
 * 
 */
void exitProgram( void )
{
    C2D_TextBufDelete(g_staticBuf);
    C2D_FontFree(font);
}

/**
 * @brief Main program
 * 
 * @return int Status code
 */
int main( void )
{
    romfsInit();
    cfguInit();

    gfxInitDefault();
    C3D_Init( C3D_DEFAULT_CMDBUF_SIZE );
    C2D_Init( C2D_DEFAULT_MAX_OBJECTS );
    C2D_Prepare();

    gfxSet3D( false );

    // hidSetRepeatParameters( 20, 1 );
    
    C3D_RenderTarget* top = C2D_CreateScreenTarget( GFX_TOP, GFX_LEFT );
    C3D_RenderTarget* bottom = C2D_CreateScreenTarget( GFX_BOTTOM, GFX_LEFT );

    C2D_SpriteSheet sky = C2D_SpriteSheetLoad( "romfs:/sky.t3x" );
    C2D_SpriteFromSheet( &day, sky, 0 );
    C2D_SpriteFromSheet( &night, sky, 1 );
    C2D_SpriteSheetFree( sky );

    g_staticBuf = C2D_TextBufNew( 4096 );
    font = C2D_FontLoadSystem( CFG_REGION_USA );

    generate();
    rainCheck(); // is it raining?

    while ( aptMainLoop() )
    {
        hidScanInput();

        u32 kDown = hidKeysDown();
        if ( kDown & KEY_START ) break;
        if ( kDown & KEY_X ) dayTime = !dayTime;

        u32 kHeld = hidKeysHeld();

        if ( kHeld & KEY_L )
        {
            scrollLeft( 30 );
            rainCheck();
        }

        if ( kHeld & KEY_R )
        {
            scrollRight( 30 );
            rainCheck();
        }

        hidCircleRead( &circlePad );

        if ( abs(circlePad.dx) > 64 )
        {
            if ( circlePad.dx > 0 )
            {
                scrollRight( circlePad.dx >> 5 );
            }
            else
            {
                scrollLeft( (-circlePad.dx) >> 5 );
            }

            rainCheck();
        }

        C3D_FrameBegin( C3D_FRAME_SYNCDRAW );

        C2D_TargetClear( top, BLACK );
        C2D_SceneBegin( top );
        renderTop();

        C2D_TargetClear( bottom, BLACK );
        C2D_SceneBegin( bottom );
        renderBottom();

        C3D_FrameEnd(0);
    }

    exitProgram();

    C2D_Fini();
    C3D_Fini();
    romfsExit();
    cfguExit();
    gfxExit();

    return 0;
}
