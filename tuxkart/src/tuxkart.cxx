
#include "tuxkart.h"

#define MIN_CAM_DISTANCE      5.0f  
#define MAX_CAM_DISTANCE     10.0f  // Was 15

int mirror = 0 ;
int player = 0 ;

int finishing_position = -1 ;

guUDPConnection *net = NULL ;
ssgLoaderOptions *loader_opts = NULL ;

int network_enabled = FALSE ;
int network_testing = FALSE ;

Herring *silver_h ;
Herring *gold_h   ;
Herring *red_h    ;
Herring *green_h  ;
 
Track     *curr_track  ;
ssgBranch *trackBranch ;

int num_herring   ;
int num_laps_in_race ;

char *trackname = "tuxtrack" ;

HerringInstance herring [ MAX_HERRING ] ;

sgCoord steady_cam ;
 
char player_files [ NUM_KARTS ][ 256 ] ;

char *traffic_files [] =
{
  "icecreamtruck.ac", "truck1.ac",
} ;


char *projectile_files [] =
{
  "spark.ac",         /* COLLECT_SPARK          */
  "missile.ac",       /* COLLECT_MISSILE        */
  "flamemissile.ac",  /* COLLECT_HOMING_MISSILE */
  NULL
} ;


char *tinytux_file   = "tinytux_magnet.ac" ;
char *explosion_file = "explode.ac"    ;
char *parachute_file = "parachute.ac"  ;
char *magnet_file    = "magnet.ac"     ;
char *magnet2_file   = "magnetbzzt.ac" ;
char *anvil_file     = "anvil.ac"      ;


ulClock      *fclock = NULL ;
SoundSystem  *sound = NULL ;
GFX            *gfx = NULL ;
GUI            *gui = NULL ;

KartDriver       *kart [ NUM_KARTS       ] ;
TrafficDriver *traffic [ NUM_TRAFFIC     ] ;
Projectile *projectile [ NUM_PROJECTILES ] ;
Explosion   *explosion [ NUM_EXPLOSIONS  ] ;

int       num_karts = 0 ;
ssgRoot      *scene = NULL ;
Track        *track = NULL ;
int      cam_follow =  0 ;
float    cam_delay  = 10.0f ;

#define MAX_FIXED_CAMERA 9

void mirror_scene ( ssgEntity *n ) ;

sgCoord fixedpos [ MAX_FIXED_CAMERA ] =
{
  { {    0,    0, 500 }, {    0, -90, 0 } },

  { {    0,  180,  30 }, {  180, -15, 0 } },
  { {    0, -180,  40 }, {    0, -15, 0 } },

  { {  300,    0,  60 }, {   90, -15, 0 } },
  { { -300,    0,  60 }, {  -90, -15, 0 } },

  { {  200,  100,  30 }, {  120, -15, 0 } },
  { {  200, -100,  40 }, {   60, -15, 0 } },
  { { -200, -100,  30 }, {  -60, -15, 0 } },
  { { -200,  100,  40 }, { -120, -15, 0 } }
} ;

Level level ;


void load_players ( char *fname )
{
  ssgEntity *obj;

  ssgEntity *pobj1 = ssgLoad ( parachute_file, loader_opts ) ;
  ssgEntity *pobj2 = ssgLoad ( magnet_file   , loader_opts ) ;
  ssgEntity *pobj3 = ssgLoad ( magnet2_file  , loader_opts ) ;
  ssgEntity *pobj4 = ssgLoad ( anvil_file    , loader_opts ) ;

  mirror_scene ( pobj1 ) ;
  mirror_scene ( pobj2 ) ;
  mirror_scene ( pobj3 ) ;
  mirror_scene ( pobj4 ) ;

  sgCoord cc ;
  sgSetCoord ( &cc, 0, 0, 2, 0, 0, 0 ) ;
  ssgTransform *ttt = new ssgTransform ( & cc ) ;
  ttt -> addKid ( ssgLoad ( tinytux_file  , loader_opts ) ) ;
  mirror_scene ( ttt ) ;

  ssgEntity *pobj5 = ttt ;
  int i ;
 
  FILE *fd = fopen ( fname, "r" ) ;

  if ( fd == NULL )
  {
    fprintf ( stderr, "tuxkart: Can't open '%s':\n", fname ) ;
    exit ( 1 ) ;
  }

  num_karts = 0 ;

  while ( num_karts < NUM_KARTS )
  {
    if ( fgets ( player_files [ num_karts ], 256, fd ) != NULL )
    {
      if ( player_files [ num_karts ][ 0 ] <= ' ' ||
           player_files [ num_karts ][ 0 ] == '#' )
        continue ;

      /* Trim off the '\n' */

      int len = strlen ( player_files [ num_karts ] ) - 1 ;

      if ( player_files [ num_karts ][ len ] <= ' ' )
        player_files [ num_karts ][ len ] = '\0' ;

      num_karts++ ;
    }
    else
      break ;
  }
 
  fclose ( fd ) ;

  if ( player >= num_karts )
    player = 0 ;

  for ( i = 0 ; i < num_karts ; i++ )
  {
    ssgRangeSelector *lod = new ssgRangeSelector ;
    float r [ 2 ] = { -10.0f, 100.0f } ;
    int kart_id ;

    if ( i == 0 )
      kart_id = player ;
    else
    if ( i == player )
      kart_id = 0 ;
    else
      kart_id = i ;
 
    obj = ssgLoad ( player_files [ kart_id ], loader_opts ) ;
    mirror_scene ( obj ) ;

    lod -> addKid ( obj ) ;
    lod -> setRanges ( r, 2 ) ;

    kart[i]-> getModel() -> addKid ( lod ) ;
    kart[i]-> addAttachment ( pobj1 ) ;
    kart[i]-> addAttachment ( pobj2 ) ;
    kart[i]-> addAttachment ( pobj3 ) ;
    kart[i]-> addAttachment ( pobj4 ) ;
    kart[i]-> addAttachment ( pobj5 ) ;
  }

  for ( i = 0 ; i < NUM_PROJECTILES ; i++ )
  {
    ssgSelector *sel = new ssgSelector ;
    projectile[i]-> getModel() -> addKid ( sel ) ;

    for ( int j = 0 ; projectile_files [ j ] != NULL ; j++ )
      sel -> addKid ( ssgLoad ( projectile_files [ j ], loader_opts ) ) ;

    mirror_scene ( sel ) ;
    projectile[i] -> off () ;
  }

  for ( i = 0 ; i < NUM_EXPLOSIONS ; i++ )
    explosion[i] = new Explosion ( (ssgBranch *) ssgLoad ( explosion_file,
                                                    loader_opts ) ) ;
}


 
static void herring_command ( char *s, char *str )
{
  if ( num_herring >= MAX_HERRING )
  {
    fprintf ( stderr, "Too many herring\n" ) ;
    return ;
  }
 
  HerringInstance *h = & herring[num_herring] ;
  sgVec3 xyz ;
 
  sscanf ( s, "%f,%f", &xyz[0], &xyz[1] ) ;
 
  xyz[2] = 1000000.0f ;
  xyz[2] = getHeight ( xyz ) + 0.06 ;
 
  sgCoord c ;
 
  sgCopyVec3 ( h->xyz, xyz ) ;
  sgSetVec3  ( c.hpr, 0.0f, 0.0f, 0.0f ) ;
  sgCopyVec3 ( c.xyz, h->xyz ) ;
 
  if ( str[0]=='Y' || str[0]=='y' ){ h->her = gold_h   ; h->type = HE_GOLD   ;}
  if ( str[0]=='G' || str[0]=='g' ){ h->her = green_h  ; h->type = HE_GREEN  ;}
  if ( str[0]=='R' || str[0]=='r' ){ h->her = red_h    ; h->type = HE_RED    ;}
  if ( str[0]=='S' || str[0]=='s' ){ h->her = silver_h ; h->type = HE_SILVER ;}
 
  h->eaten = FALSE ;
  h->scs   = new ssgTransform ;
  h->scs -> setTransform ( &c ) ;
  h->scs -> addKid ( h->her->getRoot () ) ;
  scene  -> addKid ( h->scs ) ;
 
  num_herring++ ;
}


void mirror_scene ( ssgEntity *n )
{
  if ( n == NULL || !mirror ) return ;

  n -> dirtyBSphere () ;

  if ( n -> isAKindOf ( ssgTypeLeaf() ) )
  {
    for ( int i = 0 ; i < ((ssgLeaf *)n) -> getNumVertices () ; i++ )
      ((ssgLeaf *)n) -> getVertex ( i ) [ 0 ] *= -1.0f ;

    return ;
  }

  if ( n -> isAKindOf ( ssgTypeTransform () ) )
  {
    sgMat4 xform ;

    ((ssgTransform *)n) -> getTransform ( xform ) ;
    xform [ 0 ][ 0 ] = - xform [ 0 ] [ 0 ] ;
    xform [ 1 ][ 0 ] = - xform [ 1 ] [ 0 ] ;
    xform [ 2 ][ 0 ] = - xform [ 2 ] [ 0 ] ;
    xform [ 3 ][ 0 ] = - xform [ 3 ] [ 0 ] ;
    ((ssgTransform *)n) -> setTransform ( xform ) ;
  }

  ssgBranch *b = (ssgBranch *) n ;

  for ( int i = 0 ; i < b -> getNumKids () ; i++ )
    mirror_scene ( b -> getKid ( i ) ) ;
}


void load_track ( char *fname )
{
  FILE *fd = fopen ( fname, "r" ) ;
  char playersfname [ 256 ] ;

  strcpy ( playersfname, "data/players.dat" ) ;

  if ( fd == NULL )
  {
    fprintf ( stderr, "tuxkart: Can't open track file '%s'\n", fname ) ;
    exit ( 1 ) ;
  }

  init_hooks () ;

  char s [ 1024 ] ;

  while ( fgets ( s, 1023, fd ) != NULL )
  {
    if ( *s == '#' || *s < ' ' )
      continue ;

    int need_hat = FALSE ;
    int fit_skin = FALSE ;
    char fname [ 1024 ] ;
    sgCoord loc ;
    sgZeroVec3 ( loc.xyz ) ;
    sgZeroVec3 ( loc.hpr ) ;

    char htype = '\0' ;

    if ( sscanf ( s, "PLAYERS \"%[^\"]\"", fname ) == 1 )
    {
      strcpy ( playersfname, fname ) ;
    }
    else
    if ( sscanf ( s, "MUSIC \"%[^\"]\"", fname ) == 1 )
    {
      sound -> change_track ( fname ) ;
    }
    else
    if ( sscanf ( s, "%cHERRING,%f,%f", &htype,
                     &(loc.xyz[0]), &(loc.xyz[1]) ) == 3 )
    {
      herring_command ( & s [ strlen ( "*HERRING," ) ], s ) ;
    }
    else
    if ( s[0] == '\"' )
    {
      if ( sscanf ( s, "\"%[^\"]\",%f,%f,%f,%f,%f,%f",
		 fname, &(loc.xyz[0]), &(loc.xyz[1]), &(loc.xyz[2]),
			&(loc.hpr[0]), &(loc.hpr[1]), &(loc.hpr[2]) ) == 7 )
      {
	/* All 6 DOF specified */
	need_hat = FALSE ;
      }
      else 
      if ( sscanf ( s, "\"%[^\"]\",%f,%f,{},%f,%f,%f",
		   fname, &(loc.xyz[0]), &(loc.xyz[1]),
			  &(loc.hpr[0]), &(loc.hpr[1]), &(loc.hpr[2]) ) == 6 )
      {
	/* All 6 DOF specified - but need height */
	need_hat = TRUE ;
      }
      else 
      if ( sscanf ( s, "\"%[^\"]\",%f,%f,%f,%f",
		   fname, &(loc.xyz[0]), &(loc.xyz[1]), &(loc.xyz[2]),
			  &(loc.hpr[0]) ) == 5 )
      {
	/* No Roll/Pitch specified - assumed zero */
	need_hat = FALSE ;
      }
      else 
      if ( sscanf ( s, "\"%[^\"]\",%f,%f,{},%f,{},{}",
		   fname, &(loc.xyz[0]), &(loc.xyz[1]), &(loc.hpr[0]) ) == 3 )
      {
	/* All 6 DOF specified - but need height, roll, pitch */
	need_hat = TRUE ;
	fit_skin = TRUE ;
      }
      else 
      if ( sscanf ( s, "\"%[^\"]\",%f,%f,{},%f",
		   fname, &(loc.xyz[0]), &(loc.xyz[1]),
			  &(loc.hpr[0]) ) == 4 )
      {
	/* No Roll/Pitch specified - but need height */
	need_hat = TRUE ;
      }
      else 
      if ( sscanf ( s, "\"%[^\"]\",%f,%f,%f",
		   fname, &(loc.xyz[0]), &(loc.xyz[1]), &(loc.xyz[2]) ) == 4 )
      {
	/* No Heading/Roll/Pitch specified - but need height */
	need_hat = FALSE ;
      }
      else 
      if ( sscanf ( s, "\"%[^\"]\",%f,%f,{}",
		   fname, &(loc.xyz[0]), &(loc.xyz[1]) ) == 3 )
      {
	/* No Roll/Pitch specified - but need height */
	need_hat = TRUE ;
      }
      else 
      if ( sscanf ( s, "\"%[^\"]\",%f,%f",
		   fname, &(loc.xyz[0]), &(loc.xyz[1]) ) == 3 )
      {
	/* No Z/Heading/Roll/Pitch specified */
	need_hat = FALSE ;
      }
      else 
      if ( sscanf ( s, "\"%[^\"]\"", fname ) == 1 )
      {
	/* Nothing specified */
	need_hat = FALSE ;
      }
      else
      {
	fprintf ( stderr, "tuxkart: Syntax error in '%s':\n", fname ) ;
	fprintf ( stderr, "  %s\n", s ) ;
	exit ( 1 ) ;
      }

      if ( need_hat )
      {
	sgVec3 nrm ;

	loc.xyz[2] = 1000.0f ;
	loc.xyz[2] = getHeightAndNormal ( loc.xyz, nrm ) ;

	if ( fit_skin )
	{
	  float sy = sin ( -loc.hpr [ 0 ] * SG_DEGREES_TO_RADIANS ) ;
	  float cy = cos ( -loc.hpr [ 0 ] * SG_DEGREES_TO_RADIANS ) ;
   
	  loc.hpr[2] =  SG_RADIANS_TO_DEGREES * atan2 ( nrm[0] * cy -
							nrm[1] * sy, nrm[2] ) ;
	  loc.hpr[1] = -SG_RADIANS_TO_DEGREES * atan2 ( nrm[1] * cy +
							nrm[0] * sy, nrm[2] ) ; 
	}
      }

      ssgEntity        *obj   = ssgLoad ( fname, loader_opts ) ;
      ssgRangeSelector *lod   = new ssgRangeSelector ;
      ssgTransform     *trans = new ssgTransform ( & loc ) ;

      float r [ 2 ] = { -10.0f, 2000.0f } ;

      lod         -> addKid    ( obj   ) ;
      trans       -> addKid    ( lod   ) ;
      trackBranch -> addKid    ( trans ) ;
      lod         -> setRanges ( r, 2  ) ;
    }
    else
    {
      fprintf ( stderr, "tuxkart: Syntax error in '%s':\n", fname ) ;
      fprintf ( stderr, "  %s\n", s ) ;
      exit ( 1 ) ;
    }
  }

#ifdef SSG_BACKFACE_COLLISIONS_SUPPORTED
  ssgSetBackFaceCollisions ( mirror ) ;
#endif

  mirror_scene ( trackBranch ) ;

  fclose ( fd ) ;

  sgSetVec3  ( steady_cam.xyz, 0.0f, 0.0f, 0.0f ) ;
  sgSetVec3  ( steady_cam.hpr, 0.0f, 0.0f, 0.0f ) ;

  load_players ( playersfname ) ;
}



static void banner ()
{
  printf ( "\n\n" ) ;
  printf ( "   TUXEDO T. PENGUIN stars in TUXKART!\n" ) ;
  printf ( "               by Steve and Oliver Baker\n" ) ;
  printf ( "                 <sjbaker1@airmail.net>\n" ) ;
  printf ( "                  http://tuxkart.sourceforge.net\n" ) ;
  printf ( "\n\n" ) ;
}

static void cmdline_help ()
{
  banner () ;

  printf ( "Usage:\n\n" ) ;
  printf ( "    tuxkart [OPTIONS] [machine_name]\n\n" ) ;
  printf ( "Options:\n" ) ;
  printf ( "  -h     Display this help message.\n" ) ;
  printf ( "  -t     Run a network test.\n" ) ;
  printf ( "\n" ) ;
}


int tuxkartMain ( int _numLaps, int _mirror, char *_levelName )
{
  /* Initialise some horrid globals */

  fclock           = new ulClock ;
  mirror           = _mirror     ;
  num_laps_in_race = _numLaps    ;
  trackname        = _levelName  ;

  /* Network initialisation -- NOT WORKING YET */

  net              = new guUDPConnection ;
  network_testing  = FALSE ;
  network_enabled  = FALSE ;

#ifdef ENABLE_NETWORKING
  network_enabled  = TRUE ;
  net->connect ( argv[i] ) ;

  if ( network_enabled && network_testing )
  {
    fprintf ( stderr, "You'll need to run this program\n" ) ;
    fprintf ( stderr, "on the other machine too\n" ) ;
    fprintf ( stderr, "Type ^C to exit.\n" ) ;

    while ( 1 )
    {
      char buffer [ 20 ] ;

      secondSleep ( 1 ) ;

      if ( net->recvMessage( buffer, 20 ) > 0 )
	fprintf ( stderr, "%s\n", buffer ) ;
      else
	fprintf ( stderr, "*" ) ;

      net->sendMessage ( "Testing...", 11 ) ;
    }
  }
#endif

  /* Set the SSG loader options */

  loader_opts = new ssgLoaderOptions () ;
  loader_opts -> setCreateStateCallback  ( getAppState ) ;
  loader_opts -> setCreateBranchCallback ( process_userdata ) ;
  ssgSetCurrentOptions ( loader_opts ) ;
  ssgModelPath         ( "models" ) ;
  ssgTexturePath       ( "images" ) ;


  /* Say "Hi!" to the nice user. */

  banner () ;

  /* Grab the track centerline file */

  char fname [ 100 ] ;
  sprintf ( fname, "data/%s.drv", trackname ) ;

  curr_track = new Track ( fname, mirror ) ;
  gfx        = new GFX ( mirror ) ;
  sound      = new SoundSystem ;
  gui        = new GUI ;

  pwSetCallbacks ( keystroke, mousefn, motionfn, reshape, NULL ) ;

  /* Start building the scene graph */

  scene       = new ssgRoot   ;
  trackBranch = new ssgBranch ;
  scene -> addKid ( trackBranch ) ;

  /* Load the Herring */

  sgVec3 cyan   = { 0.4, 1.0, 1.0 } ;
  sgVec3 yellow = { 1.0, 1.0, 0.4 } ;
  sgVec3 red    = { 0.8, 0.0, 0.0 } ;
  sgVec3 green  = { 0.0, 0.8, 0.0 } ;
 
  silver_h  = new Herring ( cyan   ) ;
  gold_h    = new Herring ( yellow ) ;
  red_h     = new Herring ( red    ) ;
  green_h   = new Herring ( green  ) ;

  /* Load the Karts */

  for ( int i = 0 ; i < NUM_KARTS ; i++ )
  {
    /* Kart[0] is always the player. */

    if ( i == 0 )
      kart[i] = new PlayerKartDriver  ( i, new ssgTransform ) ;
    else
    if ( network_enabled )
      kart[i] = new NetworkKartDriver ( i, new ssgTransform ) ;
    else
      kart[i] = new AutoKartDriver    ( i, new ssgTransform ) ;

    scene -> addKid ( kart[i] -> getModel() ) ;
    kart[i] -> getModel()->clrTraversalMaskBits(SSGTRAV_ISECT|SSGTRAV_HOT);
  }

  /* Load the Projectiles */

  for ( int j = 0 ; j < NUM_PROJECTILES ; j++ )
  {
    projectile[j] = new Projectile ( new ssgTransform ) ;
    scene -> addKid ( projectile[j] -> getModel() ) ;
    projectile[j]->getModel()->clrTraversalMaskBits(SSGTRAV_ISECT|SSGTRAV_HOT);
  }

  /* Load the track models */

  sprintf ( fname, "data/%s.loc", trackname ) ;
  load_track ( fname ) ;

  /* Play Ball! */

  tuxKartMainLoop () ;
  return TRUE ;
}


void updateWorld ()
{
  if ( network_enabled )
  {
    char buffer [ 1024 ] ;
    int len = 0 ;
    int got_one = FALSE ;

    while ( (len = net->recvMessage ( buffer, 1024 )) > 0 )
      got_one = TRUE ;
  
    if ( got_one )
    {
      char *p = buffer ;

      kart[1]->setCoord    ( (sgCoord *) p ) ; p += sizeof(sgCoord) ;
      kart[1]->setVelocity ( (sgCoord *) p ) ; p += sizeof(sgCoord) ;
    }
  }

  int i;
  for ( i = 0 ; i < num_karts       ; i++ ) kart       [ i ] -> update () ;
  for ( i = 0 ; i < NUM_PROJECTILES ; i++ ) projectile [ i ] -> update () ;
  for ( i = 0 ; i < NUM_EXPLOSIONS  ; i++ ) explosion  [ i ] -> update () ;

  for ( i = 0 ; i < num_karts ; i++ )
  {
    int p = 1 ;

    for ( int j = 0 ; j < num_karts ; j++ )
    {
      if ( j == i ) continue ;

      if ( kart[j]->getLap() >  kart[i]->getLap() ||
           ( kart[j]->getLap() == kart[i]->getLap() && 
             kart[j]->getDistanceDownTrack() >
                              kart[i]->getDistanceDownTrack() ))
        p++ ;      
    }

    kart [ i ] -> setPosition ( p ) ;
  }

  if ( network_enabled )
  {
    char buffer [ 1024 ] ;
    char *p = buffer ;

    memcpy ( p, kart[0]->getCoord   (), sizeof(sgCoord) ) ;
    p += sizeof(sgCoord) ;
    memcpy ( p, kart[0]->getVelocity(), sizeof(sgCoord) ) ;
    p += sizeof(sgCoord) ;

    net->sendMessage ( buffer, p - buffer ) ;
  }

  if ( cam_follow < 0 )
    cam_follow = 18 + MAX_FIXED_CAMERA - 1 ;
  else
  if ( cam_follow >= 18 + MAX_FIXED_CAMERA )
    cam_follow = 0 ;

  sgCoord final_camera ;

  if ( cam_follow < num_karts )
  {
    sgCoord cam, target, diff ;

    sgCopyCoord ( &target, kart[cam_follow]->getCoord   () ) ;
    sgCopyCoord ( &cam   , kart[cam_follow]->getHistory ( (int)cam_delay ) ) ;

    float dist = 5.0f + sgDistanceVec3 ( target.xyz, cam.xyz ) ;

    if ( dist < MIN_CAM_DISTANCE && cam_delay < 50 )
      cam_delay++ ;

    if ( dist > MAX_CAM_DISTANCE && cam_delay > 1 )
      cam_delay-- ;

    sgVec3 offset ;
    sgMat4 cam_mat ;

    sgSetVec3 ( offset, -0.5f, -5.0f, 1.5f ) ;
    sgMakeCoordMat4 ( cam_mat, &cam ) ;

    sgXformPnt3 ( offset, cam_mat ) ;

    sgCopyVec3 ( cam.xyz, offset ) ;

    cam.hpr[1] = -5.0f ;
    cam.hpr[2] = 0.0f;

    sgSubVec3 ( diff.xyz, cam.xyz, steady_cam.xyz ) ;
    sgSubVec3 ( diff.hpr, cam.hpr, steady_cam.hpr ) ;

    while ( diff.hpr[0] >  180.0f ) diff.hpr[0] -= 360.0f ;
    while ( diff.hpr[0] < -180.0f ) diff.hpr[0] += 360.0f ;
    while ( diff.hpr[1] >  180.0f ) diff.hpr[1] -= 360.0f ;
    while ( diff.hpr[1] < -180.0f ) diff.hpr[1] += 360.0f ;
    while ( diff.hpr[2] >  180.0f ) diff.hpr[2] -= 360.0f ;
    while ( diff.hpr[2] < -180.0f ) diff.hpr[2] += 360.0f ;

    steady_cam.xyz[0] += 0.2f * diff.xyz[0] ;
    steady_cam.xyz[1] += 0.2f * diff.xyz[1] ;
    steady_cam.xyz[2] += 0.2f * diff.xyz[2] ;
    steady_cam.hpr[0] += 0.1f * diff.hpr[0] ;
    steady_cam.hpr[1] += 0.1f * diff.hpr[1] ;
    steady_cam.hpr[2] += 0.1f * diff.hpr[2] ;

    final_camera = steady_cam ;
  }
  else
  if ( cam_follow < num_karts + MAX_FIXED_CAMERA )
  {
    final_camera = fixedpos[cam_follow-num_karts] ;
  }
  else
    final_camera = steady_cam ;

  sgVec3 interfovealOffset ;
  sgMat4 mat ;

  sgSetVec3 ( interfovealOffset, 0.2 * (float)stereoShift(), 0, 0 ) ;
  sgMakeCoordMat4 ( mat, &final_camera ) ;
  sgXformPnt3 ( final_camera.xyz, interfovealOffset, mat ) ;

  ssgSetCamera ( &final_camera ) ;
}



void tuxKartMainLoop ()
{
  /* Loop forever updating everything */

  while ( 1 )
  {
    /* Stop updating if we are paused */

    if ( ! gui -> isPaused () )
    {
      fclock->update () ;
      updateWorld () ;

      for ( int i = 0 ; i < MAX_HERRING ; i++ )
        if ( herring [ i ] . her != NULL )
          herring [ i ] . update () ;

      silver_h -> update () ;
      gold_h   -> update () ;
      red_h    -> update () ;
      green_h  -> update () ;

      update_hooks () ;
    }

    /* Routine stuff we do even when paused */

    gfx    -> update () ;
    gui    -> update () ;
    sound  -> update () ;
    gfx    -> done   () ;  /* Swap graphics buffers last! */
  }
}


