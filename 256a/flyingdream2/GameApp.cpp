/*
 *  GameApp.cpp
 *  SDL Test
 *
 *  Created by Mark Szymczyk on 5/1/06.
 *  Copyright 2006 Me and Mark Publishing. All rights reserved.
 *
 */

// TODO:
// add reverb (via rtaudio + stk OR fmod)
//  - look up all the fmod mode flags
//  - try fmod again with writing twice as many samples (might have been writing half as many as necessary)
// figure out "velocity out of bounds" error messages
// Bound position within skybox
// redo sky & ocean drawing to draw everything correctly (will need environment-specific rendering code)
// add ground
// add clouds
// better lighting (as if there's a sun?
// look into fmod as sound engine (sound come from various env elements?)
// draw tubes correctly (make them correct in all directions)
// make tubes look neat (with "random" (aka wind wisps from wii zelda) lines around it?)
// bigger, more interesting environment

#include "BiQuad.h"
#include "Noise.h"
#include "SineWave.h"
#include "NRev.h"

#include "GameApp.h"
#include <iostream>
#include <stdlib.h>
using namespace std;

const float PI       =  3.14159265358979323846f;
#define	DEGTORAD(x)	( ((x) * PI) / 180.0 )
#define	RADTODEG(x)	( ((x) * 180.0) / PI )

#define SAMPLE float

static fluid_settings_t* settings;
static fluid_synth_t* synth;

stk::Noise noise;
stk::NRev reverb;
stk::BiQuad biquad;

/*
void ERRCHECK(FMOD_RESULT result)
{
    if (result != FMOD_OK)
    {
        printf("FMOD error! (%d) %s\n", result, FMOD_ErrorString(result));
        exit(-1);
    }
}

FMOD_RESULT F_CALLBACK pcmreadcallback(FMOD_SOUND *sound, void *data, unsigned int datalen)
{
    unsigned int  count;
    static float  t1 = 0, t2 = 0;        // time
    static float  v1 = 0, v2 = 0;        // velocity
    signed short *stereo16bitbuffer = (signed short *)data;
    
    fluid_synth_write_s16(synth, datalen >> 1, stereo16bitbuffer, 0, 2, stereo16bitbuffer, 1, 2);
    
    
    for (count=0; count < datalen >> 2; count++)        // >>2 = 16bit stereo (4 bytes per sample)
    {
        *stereo16bitbuffer++ = (signed short)(sin(t1) * 32767.0f);    // left channel
        *stereo16bitbuffer++ = (signed short)(sin(t2) * 32767.0f);    // right channel
        
        t1 += 0.01f   + v1;
        t2 += 0.0142f + v2;
        v1 += (float)(sin(t1) * 0.002f);
        v2 += (float)(sin(t2) * 0.002f);
    }
    
    return FMOD_OK;
}


FMOD_RESULT F_CALLBACK pcmsetposcallback(FMOD_SOUND *sound, int subsound, unsigned int position, FMOD_TIMEUNIT postype)
{

    // This is useful if the user calls Sound::setPosition and you want to seek your data accordingly.
    return FMOD_OK;
}
*/


int AudioCallback( void *outputBuffer, void *inputBuffer, unsigned int nBufferFrames,
                  double streamTime, RtAudioStreamStatus status, void *userData )
{
    //struct synths *s = (synths*)userData;
    //stk::Noise *noise = s->noise;
    
    //printf("at start of callback, noise's lastFrame_ size is now %d\n", noise.lastFrame().size());

    
    
    
    SAMPLE *buffer = (SAMPLE *) outputBuffer;    
    if ( status )
        std::cout << "Stream underflow detected!" << std::endl;
    
    fluid_synth_write_float(synth, nBufferFrames, buffer, 0, 2, buffer, 1, 2);

    // TODO: FIGURE OUT A WAY TO DO THIS IN PLACE
    //stk::StkFrames temp_buffer(nBufferFrames, 2);

    /*
    for (int i = 0; i < nBufferFrames; i++) {
        temp_buffer[i*2] = (stk::StkFloat)buffer[i*2];
        temp_buffer[i*2+1] = (stk::StkFloat)buffer[i*2+1];
    }
    reverb->tick(temp_buffer, 0);
    for (int i = 0; i < nBufferFrames; i++) {
        buffer[i*2] = (SAMPLE)temp_buffer[i*2];
        buffer[i*2+1] = (SAMPLE)temp_buffer[i*2+1];
    }
     */
    
    //stk::BiQuad biquad;
    //biquad.setResonance( 440.0, 0.98, true ); // automatically normalize for unity peak gain
    
    
    stk::StkFloat l_output_sample, r_output_sample;
    for ( unsigned int i=0; i<nBufferFrames; i++ ) {
        //output_sample = noise.tick();

        l_output_sample = buffer[i*2];
        l_output_sample = reverb.tick(l_output_sample, 0);

        r_output_sample = buffer[i*2+1];
        r_output_sample = reverb.lastFrame()[1];//reverb.tick(r_output_sample, 0);

        //output_sample = reverb.tick(output_sample, 0);
        buffer[i*2] = l_output_sample;  // single-sample computations
        buffer[i*2+1] = r_output_sample;  // single-sample computations
        //temp_buffer[i] = biquad.tick( noise.tick() );  // single-sample computations
        //std::cout << "i = " << i << " : output = " << output[i] << std::endl;
    }

    
    return 0;
}


// Constructor
GameApp::GameApp(void)
{
    //printf("HOW BIG IS STKFLOAT? %d\n", sizeof(stk::StkFloat));
    
    
    
    stk::Stk::setSampleRate( 44100.0 );
    //sine = new stk::SineWave();
    //sine->setFrequency(220);
    //noise = new stk::Noise();
    //reverb = new stk::NRev();
    //printf("noise's lastFrame_ size is now %d\n", noise->lastFrame().size());
    
    biquad.setResonance( 440.0, 0.98, true );

    
    g_width = 1440;
    g_height = 855;
    g_pitch = g_yaw = g_roll = 0;
    g_tracking = false;
    done = false;
    g_forward_held = false;
    g_backward_held = false;
    
    g_gravity = Vector3D(0, 0.05, 0);
    g_gravity_on = true;
    
    g_cuboids = new vector<Cuboid *>();
    
    vector<int> *scale_degrees = new vector<int>();

    /*
    // Major scale
    scale_degrees->push_back(2);
     scale_degrees->push_back(2);
    scale_degrees->push_back(1);
    scale_degrees->push_back(2);
    scale_degrees->push_back(2);
    scale_degrees->push_back(2);
    scale_degrees->push_back(1);
     */
    
    // Major Pentatonic
    scale_degrees->push_back(3);
    scale_degrees->push_back(2);
    scale_degrees->push_back(2);
    scale_degrees->push_back(3);
    scale_degrees->push_back(2);
    
    /*
    // Octave
    scale_degrees->push_back(12);
    */
    
    g_pitch_mapper = new PitchMapper(60, scale_degrees);
    
    
    
    
    settings = new_fluid_settings();
    
    fluid_settings_setint(settings, "synth.midi-channels", 128);
    synth = new_fluid_synth(settings);
    
    //fluid_settings_setstr(settings, "audio.driver", "coreaudio");
    //adriver = new_fluid_audio_driver(settings, synth);
    
    fluid_synth_sfload(synth, "/Users/mrotondo/Downloads/TimGM6mb.sf2", 1);
    
    
    
    /*
    FMOD_RESULT result;
    result = FMOD::System_Create(&system);
    ERRCHECK(result);
    
    unsigned int version;
    result = system->getVersion(&version);
    ERRCHECK(result);
    printf("Using fmod version %08x\n", version);
    
    result = system->init(32, FMOD_INIT_NORMAL, 0);
    ERRCHECK(result);
    
    
    FMOD_MODE mode = FMOD_2D | FMOD_OPENUSER | FMOD_LOOP_NORMAL | FMOD_HARDWARE | FMOD_CREATESTREAM;
    int channels = 2;
    FMOD::Sound *sound;
    
    FMOD_CREATESOUNDEXINFO  createsoundexinfo;
    memset(&createsoundexinfo, 0, sizeof(FMOD_CREATESOUNDEXINFO));
    createsoundexinfo.cbsize            = sizeof(FMOD_CREATESOUNDEXINFO);              // required.
    createsoundexinfo.decodebuffersize  = 10024;                                       // Chunk size of stream update in samples.  This will be the amount of data passed to the user callback.

    // ??? 4 works, huge # also works, need to test with load
    createsoundexinfo.length            = 44100 * channels * sizeof(signed short) * 2; // Length of PCM data in bytes of whole song (for Sound::getLength) 

    // sure, whatever.
    createsoundexinfo.numchannels       = channels;                                    // Number of channels in the sound.
    createsoundexinfo.defaultfrequency  = 44100;                                       // Default playback rate of sound. 
    createsoundexinfo.format            = FMOD_SOUND_FORMAT_PCM16;                     // Data format of sound. 
    createsoundexinfo.pcmreadcallback   = pcmreadcallback;                             // User callback for reading. 
    createsoundexinfo.pcmsetposcallback = pcmsetposcallback;                           // User callback for seeking. 
    
    result = system->createSound(0, mode, &createsoundexinfo, &sound);
    ERRCHECK(result);
    
    FMOD::Channel *channel = 0;
    result = system->playSound(FMOD_CHANNEL_FREE, sound, 0, &channel);
    ERRCHECK(result);
    */
    

    
    
    if ( dac.getDeviceCount() < 1 ) {
        std::cout << "\nNo audio devices found!\n";
        exit( 0 );
    }
    
    RtAudio::StreamParameters parameters;
    parameters.deviceId = dac.getDefaultOutputDevice();
    parameters.nChannels = 2;
    parameters.firstChannel = 0;
    unsigned int sampleRate = 44100;
    unsigned int bufferFrames = 8192;//256; // 256 sample frames
    double data[2];
    
    //printf("just before setting up callback, noise's lastFrame_ size is now %d\n", noise->lastFrame().size());
    
    //void *v_s = malloc(sizeof(synths));
    //synths *s = (synths *)v_s;
    //s->noise = noise;
    
    try {
        dac.openStream(&parameters, 
                       NULL, 
                       RTAUDIO_FLOAT32,
                       sampleRate, 
                       &bufferFrames, 
                       &AudioCallback, 
                       NULL);
                       //(void *)&s );
        dac.startStream();
    }
    catch ( RtError& e ) {
        e.printMessage();
        exit( 0 );
    }
 
    
    //printf("just after setting up callback, noise's lastFrame_ size is now %d\n", noise->lastFrame().size());
    
}

// Destructor
GameApp::~GameApp(void)
{
    /*
    delete_fluid_synth(synth);
    delete_fluid_settings(settings);
    
    try {
        // Stop the stream
        dac.stopStream();
    }
    catch (RtError& e) {
        e.printMessage();
    }
    
    if ( dac.isStreamOpen() ) dac.closeStream();
    */
}

void GameApp::AddCuboid(float x, float y, float z)
{
    Vector3D *color = new Vector3D(rand() / (double)RAND_MAX,
                                   rand() / (double)RAND_MAX,
                                   rand() / (double)RAND_MAX);
    Cuboid *new_cuboid = new Cuboid(new Vector3D(x, y, z), color, 1.0);
    g_cuboids->push_back(new_cuboid);
}

// Initialization functions
void GameApp::InitApp(void)
{
    for (int i = 0; i < 1000; i++) {
        AddCuboid(1000 - 2000.0 * rand() / RAND_MAX, 
                  1000 - 2000.0 * rand() / RAND_MAX, 
                  1000 - 2000.0 * rand() / RAND_MAX);
    }
    
    g_skybox = new Skybox(new Vector3D(0, 0, 0),
                          new Vector3D(127/255.0, 178/255.0, 240/255.0),
                          1000);
    g_ocean = new Cuboid(new Vector3D(0, -1500, 0),
                         new Vector3D(53/255.0, 71/255.0, 140/255.0),
                         990);
    
    InitializeSDL();
    InstallTimer();
    SDL_ShowCursor(SDL_DISABLE);
    
}

void GameApp::InitializeSDL(void)
{
    int error;
    SDL_Surface* drawContext;
    
    error = SDL_Init(SDL_INIT_EVERYTHING);
    
    // Create a double-buffered draw context
    SDL_GL_SetAttribute(SDL_GL_DOUBLEBUFFER, 1);
    SDL_GL_SetAttribute(SDL_GL_DEPTH_SIZE, 32);
    
    
        
    Uint32 flags;
    flags = SDL_OPENGL; // | SDL_FULLSCREEN;
    drawContext = SDL_SetVideoMode(g_width, g_height, 0, flags);
    

    glViewport( 0, 0, ( GLsizei )g_width, ( GLsizei )g_height );
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    gluPerspective(45.0, g_width/g_height, 0.1, 4000);
    
    glMatrixMode(GL_MODELVIEW);
    glLoadIdentity();
    
    gluLookAt(0, 0, 0, 0, 0, -1, 0, 1, 0);
    
    glEnable(GL_DEPTH_TEST);
    glDepthFunc(GL_LEQUAL);
    glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);
    
    glEnable(GL_BLEND);
    glEnable(GL_POLYGON_SMOOTH);
    glShadeModel(GL_SMOOTH);
    glBlendFunc(GL_SRC_ALPHA, GL_ONE_MINUS_SRC_ALPHA);
    
    glHint(GL_POLYGON_SMOOTH, GL_NICEST);
    
    
    
    
    GLfloat mat_specular[] = { 1.0, 1.0, 1.0, 1.0 };
    GLfloat mat_shininess[] = { 50.0 };

    glMaterialfv(GL_FRONT, GL_SPECULAR, mat_specular);
    glMaterialfv(GL_FRONT, GL_SHININESS, mat_shininess);
    
    glEnable(GL_LIGHTING);
    //glEnable(GL_LIGHT0);
    
    //glEnable(GL_COLOR_MATERIAL);
    //GLfloat LightAmbient[]= { 0.5f, 0.5f, 0.5f, 1.0f }; 				// Ambient Light Values ( NEW )
    //glLightfv(GL_LIGHT1, GL_AMBIENT, LightAmbient);				// Setup The Ambient Light
    GLfloat LightDiffuse[]= { 1.0f, 1.0f, 1.0f, 0.5f };				 // Diffuse Light Values ( NEW )
    glLightfv(GL_LIGHT1, GL_DIFFUSE, LightDiffuse);				// Setup The Diffuse Light
    glEnable(GL_LIGHT1);							// Enable Light One
    
    GLfloat light_position[] = { 0.0, 0.0, 1.0, 0.0 };
    glLightfv(GL_LIGHT1, GL_POSITION, light_position);
    
    /*
    GLfloat fogColor[4]= {127/255.0, 178/255.0, 240/255.0, 1.0f};		// Fog Color
    glFogi(GL_FOG_MODE, GL_LINEAR);		// Fog Mode
    glFogfv(GL_FOG_COLOR, fogColor);			// Set Fog Color
    glFogf(GL_FOG_DENSITY, 0.35f);				// How Dense Will The Fog Be
    glHint(GL_FOG_HINT, GL_DONT_CARE);			// Fog Hint Value
    glFogf(GL_FOG_START, 0.0f);				// Fog Start Depth
    glFogf(GL_FOG_END, 1500.0f);				// Fog End Depth
    //glEnable(GL_FOG);					// Enables GL_FOG
     */
}

void GameApp::InstallTimer(void)
{
    timer = SDL_AddTimer(30, GameLoopTimer, this);
}

Uint32 GameApp::GameLoopTimer(Uint32 interval, void* param)
{
    // Create a user event to call the game loop.
    SDL_Event event;
    
    event.type = SDL_USEREVENT;
    event.user.code = RUN_GAME_LOOP;
    event.user.data1 = 0;
    event.user.data2 = 0;
    
    SDL_PushEvent(&event);
    
    return interval;
}


// Cleanup functions
void GameApp::Cleanup(void)
{
    SDL_bool success;
    success = SDL_RemoveTimer(timer);
    
    SDL_Quit();
}


// Event-related functions
void GameApp::EventLoop(void)
{
    SDL_Event event;
    
    vector<Cuboid *>::iterator itr;
    Cuboid *cuboid;

    while((!done) && (SDL_WaitEvent(&event))) {
        switch(event.type) {
            case SDL_USEREVENT:
                HandleUserEvents(&event);
                break;
                
            case SDL_MOUSEMOTION:
                if (abs(event.motion.xrel) > 50 || abs(event.motion.yrel) > 50) {
                    break;
                }
                g_yaw += event.motion.xrel * 0.3;                
                g_pitch += event.motion.yrel * 0.3;
                
                if (g_pitch > 90) g_pitch = 90;
                if (g_pitch < -90) g_pitch = -90;
                
                if (event.motion.x < 10 || event.motion.x > g_width - 10 ||
                    event.motion.y < 10 || event.motion.y > g_height - 10) {
                    SDL_WarpMouse(g_width / 2, g_height / 2);
                    SDL_ShowCursor(SDL_DISABLE);
                }
                break;
            case SDL_KEYDOWN:
                switch (event.key.keysym.sym) {
                    case SDLK_q:
                        done = true;
                        break;
                    default:
                        break;
                    case SDLK_SPACE:
                        itr=g_cuboids->begin();                
                        while(itr != g_cuboids->end()) {
                            cuboid = *itr;                            
                            cuboid->Flash();
                            ++itr;
                        }
                        g_tracking = true;
                        g_paths.push_back(new Path());
                        break;
                    case SDLK_w:
                        g_forward_held = true;
                        break;
                    case SDLK_s:
                        g_backward_held = true;
                        break;
                    case SDLK_g:
                        g_gravity_on = g_gravity_on ? false : true;
                        break;
                    case SDLK_f:
                        g_gravity_on = false;
                        g_velocity = Vector3D(0, 0, 0);
                        break;
                    case SDLK_r:
                        g_position = Vector3D(0, 0, 0);
                        break;
                    case SDLK_d:
                        fluid_synth_noteon(synth, 0, 60, 100);
                        break;

                        
                }
                break;
                
            case SDL_KEYUP:
                switch (event.key.keysym.sym) {
                    case SDLK_w:
                        g_forward_held = false;
                        break;
                    case SDLK_s:
                        g_backward_held = false;
                        break;
                    case SDLK_SPACE:
                        itr=g_cuboids->begin();                
                        while(itr != g_cuboids->end()) {
                            cuboid = *itr;                            
                            cuboid->Flash();
                            ++itr;
                        }
                        g_tracking = false;
                        g_paths.back()->FinishedDrawing();
                        break;
                    case SDLK_d:
                        fluid_synth_noteoff(synth, 0, 60);
                        break;
                }
                break;

            
            case SDL_QUIT:
                done = true;
                break;
                
            default:
                break;
        }   // End switch
            
    }   // End while
        
}

void GameApp::HandleUserEvents(SDL_Event* event)
{
    switch (event->user.code) {
        case RUN_GAME_LOOP:
            GameLoop();
            break;
            
        default:
            break;
    }
}


// Game related functions
void GameApp::GameLoop(void)
{
    Vector3D force;
    static float movement_scale = 0.2;
    float rad_pitch, rad_yaw;
    
    if (g_forward_held) {
        rad_pitch = DEGTORAD(g_pitch + 90);
        rad_yaw = DEGTORAD(g_yaw);
        
        force = Vector3D(-movement_scale * sin(rad_pitch) * sin(rad_yaw),
                         -movement_scale * cos(rad_pitch),
                         movement_scale * sin(rad_pitch) * cos(rad_yaw));
        g_velocity += force;
        
        if (g_tracking) {
            g_paths.back()->AddPoint(new Vector3D(g_position));
        }
    }
    if (g_backward_held) {
        rad_pitch = DEGTORAD(g_pitch + 90);
        rad_yaw = DEGTORAD(g_yaw);
        
        force = Vector3D(movement_scale * sin(rad_pitch) * sin(rad_yaw),
                         movement_scale * cos(rad_pitch),
                         -movement_scale * sin(rad_pitch) * cos(rad_yaw));
        g_velocity += force;
    }
    
    // gravity
    if (g_gravity_on) {
        g_velocity += g_gravity;
    }
    // air resistance
    g_velocity *= 0.99;
    //movement
    g_position += g_velocity * 0.5;
    if (g_tracking) {
        g_paths.back()->AddPoint(new Vector3D(g_position));
    }
    
    
    // Play paths
    deque<Path *>::iterator path_itr=g_paths.begin();
    while(path_itr != g_paths.end()) {
        Path *path = *path_itr;
        path->Update();
        
        path->Play(synth, g_pitch_mapper);
        ++path_itr;
    }
    while (!g_paths.empty() && (*g_paths.begin())->Done()) {
        g_paths.pop_front();
    }
    
    RenderFrame();    
}

void GameApp::RenderFrame(void) 
{
    glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    glPushMatrix();
    glLoadIdentity();
    
    glRotatef(g_pitch, 1, 0, 0);		// multiply into matrix
    glRotatef(g_yaw, 0, 1, 0);
    glTranslatef(g_position.x, g_position.y, g_position.z);

    GLfloat light_position[] = { 100.0, 100.0, 100.0, 0.0 };
    glLightfv(GL_LIGHT1, GL_POSITION, light_position);
    
    g_ocean->Render();
    
    g_skybox->Render();
    
    //printf("num cuboids: %d\n", g_cuboids->size());
    
    vector<Cuboid *>::iterator itr=g_cuboids->begin();
    while(itr != g_cuboids->end()) {
        Cuboid *cuboid = *itr;

        if (cuboid == NULL)
            printf("CUBOID IS NULL\n");
        
        cuboid->Render();
        ++itr;
    }
    
    deque<Path *>::iterator path_itr=g_paths.begin();
    while(path_itr != g_paths.end()) {
        Path *path = *path_itr;
        
        path->Render();
        
        ++path_itr;
    }
    
    glPopMatrix();

    SDL_GL_SwapBuffers();
}