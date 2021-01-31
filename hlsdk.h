
#define VectorMul(vec,num,res) { (res)[0] = (vec)[0] * (num); (res)[1] = (vec)[1] * (num); (res)[2] = (vec)[2] * (num); }
#define VectorCopy(a,b) {(b)[0]=(a)[0];(b)[1]=(a)[1];(b)[2]=(a)[2];}
#define VectorInverse(x) ((x)[0] = -(x)[0], (x)[1] = -(x)[1], (x)[2] = -(x)[2])
#define VectorScale(in, scale, out) ((out)[0] = (in)[0] * (scale),(out)[1] = (in)[1] * (scale),(out)[2] = (in)[2] * (scale))
#define VectorSubtract(a,b,c) {(c)[0]=(a)[0]-(b)[0];(c)[1]=(a)[1]-(b)[1];(c)[2]=(a)[2]-(b)[2];}
#define bound( min, num, max ) ((num) >= (min) ? ((num) < (max) ? (num) : (max)) : (min))

#include "Vector.h"
#include <Windows.h>

// edict->movetype values
#define	MOVETYPE_NONE			0		// never moves
//#define	MOVETYPE_ANGLENOCLIP	1
//#define	MOVETYPE_ANGLECLIP		2
#define	MOVETYPE_WALK			3		// Player only - moving on the ground
#define	MOVETYPE_STEP			4		// gravity, special edge handling -- monsters use this
#define	MOVETYPE_FLY			5		// No gravity, but still collides with stuff
#define	MOVETYPE_TOSS			6		// gravity/collisions
#define	MOVETYPE_PUSH			7		// no clip to world, push and crush
#define	MOVETYPE_NOCLIP			8		// No gravity, no collisions, still do velocity/avelocity
#define	MOVETYPE_FLYMISSILE		9		// extra size to monsters
#define	MOVETYPE_BOUNCE			10		// Just like Toss, but reflect velocity when contacting surfaces
#define MOVETYPE_BOUNCEMISSILE	11		// bounce w/o gravity
#define MOVETYPE_FOLLOW			12		// track movement of aiment
#define	MOVETYPE_PUSHSTEP		13		// BSP model that needs physics/world collisions (uses nearest hull for world collision)

#define	ZISCALE	((float)0x8000)

typedef struct
{
	byte r, g, b;
} color24;

typedef struct
{
	unsigned r, g, b, a;
} colorVec;

typedef int qboolean;

// Entity state is used for the baseline and for delta compression of a packet of 
//  entities that is sent to a client.
struct entity_state_s
{
	// Fields which are filled in by routines outside of delta compression
	int			entityType;
	// Index into cl_entities array for this entity.
	int			number;
	float		msg_time;

	// Message number last time the player/entity state was updated.
	int			messagenum;

	// Fields which can be transitted and reconstructed over the network stream
	vec3_t		origin;
	vec3_t		angles;

	int			modelindex;
	int			sequence;
	float		frame;
	int			colormap;
	short		skin;
	short		solid;
	int			effects;
	float		scale;

	byte		eflags;

	// Render information
	int			rendermode;
	int			renderamt;
	color24		rendercolor;
	int			renderfx;

	int			movetype;
	float		animtime;
	float		framerate;
	int			body;
	byte		controller[4];
	byte		blending[4];
	vec3_t		velocity;

	// Send bbox down to client for use during prediction.
	vec3_t		mins;
	vec3_t		maxs;

	int			aiment;
	// If owned by a player, the index of that player ( for projectiles ).
	int			owner;

	// Friction, for prediction.
	float		friction;
	// Gravity multiplier
	float		gravity;

	// PLAYER SPECIFIC
	int			team;
	int			playerclass;
	int			health;
	qboolean	spectator;
	int         weaponmodel;
	int			gaitsequence;
	// If standing on conveyor, e.g.
	vec3_t		basevelocity;
	// Use the crouched hull, or the regular player hull.
	int			usehull;
	// Latched buttons last time state updated.
	int			oldbuttons;
	// -1 = in air, else pmove entity number
	int			onground;
	int			iStepLeft;
	// How fast we are falling
	float		flFallVelocity;

	float		fov;
	int			weaponanim;

	// Parametric movement overrides
	vec3_t				startpos;
	vec3_t				endpos;
	float				impacttime;
	float				starttime;

	// For mods
	int			iuser1;
	int			iuser2;
	int			iuser3;
	int			iuser4;
	float		fuser1;
	float		fuser2;
	float		fuser3;
	float		fuser4;
	vec3_t		vuser1;
	vec3_t		vuser2;
	vec3_t		vuser3;
	vec3_t		vuser4;
};

typedef entity_state_s entity_state_t;

typedef struct
{
	// Time stamp for this movement
	float					animtime;

	vec3_t					origin;
	vec3_t					angles;
} position_history_t;

typedef struct
{
	byte					mouthopen;		// 0 = mouth closed, 255 = mouth agape
	byte					sndcount;		// counter for running average
	int						sndavg;			// running average
} mouth_t;

typedef struct
{
	float					prevanimtime;
	float					sequencetime;
	byte					prevseqblending[2];
	vec3_t					prevorigin;
	vec3_t					prevangles;

	int						prevsequence;
	float					prevframe;

	byte					prevcontroller[4];
	byte					prevblending[2];
} latchedvars_t;

#define HISTORY_MAX			64  // Must be power of 2

struct cl_entity_s
{
	int						index;      // Index into cl_entities ( should match actual slot, but not necessarily )

	qboolean				player;     // True if this entity is a "player"

	entity_state_t			baseline;   // The original state from which to delta during an uncompressed message
	entity_state_t			prevstate;  // The state information from the penultimate message received from the server
	entity_state_t			curstate;   // The state information from the last message received from server

	int						current_position;  // Last received history update index
	position_history_t		ph[HISTORY_MAX];   // History of position and angle updates for this player

	mouth_t					mouth;			// For synchronizing mouth movements.

	latchedvars_t			latched;		// Variables used by studio model rendering routines

	// Information based on interplocation, extrapolation, prediction, or just copied from last msg received.
	//
	float					lastmove;

	// Actual render position and angles
	vec3_t					origin;
	vec3_t					angles;

	// Attachment points
	vec3_t					attachment[4];

	// Other entity local information
	int						trivial_accept;

	struct model_s* model;			// cl.model_precache[ curstate.modelindes ];  all visible entities have a model
	struct efrag_s* efrag;			// linked list of efrags
	struct mnode_s* topnode;		// for bmodels, first world node that splits bmodel, or NULL if not split

	float					syncbase;		// for client-side animations -- used by obsolete alias animation system, remove?
	int						visframe;		// last frame this entity was found in an active leaf
	colorVec				cvFloorColor;
};

typedef struct cl_entity_s cl_entity_t;

typedef vec_t				vec4_t[4];

#define MAXSTUDIOBONES		128		// total bones actually used

typedef struct
{
	int					id;
	int					version;

	char				name[64];
	int					length;

	vec3_t				eyeposition;	// ideal eye position
	vec3_t				min;			// ideal movement hull size
	vec3_t				max;

	vec3_t				bbmin;			// clipping bounding box
	vec3_t				bbmax;

	int					flags;

	int					numbones;			// bones
	int					boneindex;

	int					numbonecontrollers;		// bone controllers
	int					bonecontrollerindex;

	int					numhitboxes;			// complex bounding boxes
	int					hitboxindex;

	int					numseq;				// animation sequences
	int					seqindex;

	int					numseqgroups;		// demand loaded sequences
	int					seqgroupindex;

	int					numtextures;		// raw textures
	int					textureindex;
	int					texturedataindex;

	int					numskinref;			// replaceable textures
	int					numskinfamilies;
	int					skinindex;

	int					numbodyparts;
	int					bodypartindex;

	int					numattachments;		// queryable attachable points
	int					attachmentindex;

	int					soundtable;
	int					soundindex;
	int					soundgroups;
	int					soundgroupindex;

	int					numtransitions;		// animation node to animation node transition graph
	int					transitionindex;
} studiohdr_t;

// sequence flags
#define STUDIO_LOOPING	0x0001

#define MAX_MODEL_NAME		64
#define MAX_MAP_HULLS		4
#define	MIPLEVELS			4
#define	NUM_AMBIENTS		4		// automatic ambient sounds
#define	MAXLIGHTMAPS		4
#define	PLANE_ANYZ			5

typedef struct
{
	float		mins[3], maxs[3];
	float		origin[3];
	int			headnode[MAX_MAP_HULLS];
	int			visleafs;		// not including the solid leaf 0
	int			firstface, numfaces;
} dmodel_t;

typedef enum
{
	mod_brush,
	mod_sprite,
	mod_alias,
	mod_studio
} modtype_t;

typedef enum
{
	ST_SYNC = 0,
	ST_RAND
} synctype_t;

// plane_t structure
typedef struct mplane_s
{
	vec3_t	normal;			// surface normal
	float	dist;			// closest appoach to origin
	byte	type;			// for texture axis selection and fast side tests
	byte	signbits;		// signx + signy<<1 + signz<<1
	byte	pad[2];
} mplane_t;

typedef struct
{
	vec3_t		position;
} mvertex_t;

typedef struct
{
	unsigned short	v[2];
	unsigned int	cachededgeoffset;
} medge_t;

typedef struct mnode_s
{
	// common with leaf
	int			contents;		// 0, to differentiate from leafs
	int			visframe;		// node needs to be traversed if current

	short		minmaxs[6];		// for bounding box culling

	struct mnode_s* parent;

	// node specific
	mplane_t* plane;
	struct mnode_s* children[2];

	unsigned short		firstsurface;
	unsigned short		numsurfaces;
} mnode_t;

typedef struct texture_s
{
	char		name[16];
	unsigned	width, height;
	int			anim_total;				// total tenths in sequence ( 0 = no)
	int			anim_min, anim_max;		// time for this frame min <=time< max
	struct texture_s* anim_next;		// in the animation sequence
	struct texture_s* alternate_anims;	// bmodels in frame 1 use these
	unsigned	offsets[MIPLEVELS];		// four mip maps stored
	unsigned	paloffset;
} texture_t;

typedef struct
{
	float		vecs[2][4];		// [s/t] unit vectors in world space. 
								// [i][3] is the s/t offset relative to the origin.
								// s or t = dot(3Dpoint,vecs[i])+vecs[i][3]
	float		mipadjust;		// ?? mipmap limits for very small surfaces
	texture_t* texture;
	int			flags;			// sky or slime, no lightmap or 256 subdivision
} mtexinfo_t;

typedef struct decal_s		decal_t;

struct msurface_s
{
	int			visframe;		// should be drawn when node is crossed

	int			dlightframe;	// last frame the surface was checked by an animated light
	int			dlightbits;		// dynamically generated. Indicates if the surface illumination 
								// is modified by an animated light.

	mplane_t* plane;			// pointer to shared plane			
	int			flags;			// see SURF_ #defines

	int			firstedge;	// look up in model->surfedges[], negative numbers
	int			numedges;	// are backwards edges

// surface generation data
	struct surfcache_s* cachespots[MIPLEVELS];

	short		texturemins[2]; // smallest s/t position on the surface.
	short		extents[2];		// ?? s/t texture size, 1..256 for all non-sky surfaces

	mtexinfo_t* texinfo;

	// lighting info
	byte		styles[MAXLIGHTMAPS]; // index into d_lightstylevalue[] for animated lights 
									  // no one surface can be effected by more than 4 
									  // animated lights.
	color24* samples;

	decal_t* pdecals;
};

/////////////////
// Customization
// passed to pfnPlayerCustomization
// For automatic downloading.
typedef enum
{
	t_sound = 0,
	t_skin,
	t_model,
	t_decal,
	t_generic,
	t_eventscript,
	t_world,		// Fake type for world, is really t_model
} resourcetype_t;

#define MAX_QPATH 64    // Must match value in quakedefs.h

typedef struct resource_s
{
	char              szFileName[MAX_QPATH]; // File name to download/precache.
	resourcetype_t    type;                // t_sound, t_skin, t_model, t_decal.
	int               nIndex;              // For t_decals
	int               nDownloadSize;       // Size in Bytes if this must be downloaded.
	unsigned char     ucFlags;

	// For handling client to client resource propagation
	unsigned char     rgucMD5_hash[16];    // To determine if we already have it.
	unsigned char     playernum;           // Which player index this resource is associated with, if it's a custom resource.

	unsigned char	  rguc_reserved[32]; // For future expansion
	struct resource_s* pNext;              // Next in chain.
	struct resource_s* pPrev;
} resource_t;

typedef struct customization_s
{
	qboolean bInUse;     // Is this customization in use;
	resource_t resource; // The resource_t for this customization
	qboolean bTranslated; // Has the raw data been translated into a useable format?  
						   //  (e.g., raw decal .wad make into texture_t *)
	int        nUserData1; // Customization specific data
	int        nUserData2; // Customization specific data
	void* pInfo;          // Buffer that holds the data structure that references the data (e.g., the cachewad_t)
	void* pBuffer;       // Buffer that holds the data for the customization (the raw .wad data)
	struct customization_s* pNext; // Next in chain
} customization_t;

#define	MAX_INFO_STRING			256
#define	MAX_SCOREBOARDNAME		32


typedef struct player_info_s
{
	// User id on server
	int		userid;

	// User info string
	char	userinfo[MAX_INFO_STRING];

	// Name
	char	name[MAX_SCOREBOARDNAME];

	// Spectator or not, unused
	int		spectator;

	int		ping;
	int		packet_loss;

	// skin information
	char	model[MAX_QPATH];
	int		topcolor;
	int		bottomcolor;

	// last frame rendered
	int		renderframe;

	// Gait frame estimation
	int		gaitsequence;
	float	gaitframe;
	float	gaityaw;
	vec3_t	prevgaitorigin;

	customization_t customdata;
} player_info_t;


// studio models
typedef struct
{
	char				name[64];

	int					type;

	float				boundingradius;

	int					nummesh;
	int					meshindex;

	int					numverts;		// number of unique vertices
	int					vertinfoindex;	// vertex bone info
	int					vertindex;		// vertex vec3_t
	int					numnorms;		// number of unique surface normals
	int					norminfoindex;	// normal bone info
	int					normindex;		// normal vec3_t

	int					numgroups;		// deformation groups
	int					groupindex;
} mstudiomodel_t;

#define MAX_CLIENTS			32

// body part index
typedef struct
{
	char				name[64];
	int					nummodels;
	int					base;
	int					modelindex; // index into models array
} mstudiobodyparts_t;

typedef struct cvar_s
{
	char* name;
	char* string;
	int		flags;
	float	v;
	struct cvar_s* next;
} cvar_t;

// JAY: Compress this as much as possible
struct decal_s
{
	decal_t* pnext;			// linked list for each surface
	msurface_s* psurface;		// Surface id for persistence / unlinking
	short		dx;				// Offsets into surface texture (in texture coordinates, so we don't need floats)
	short		dy;
	short		texture;		// Decal texture
	byte		scale;			// Pixel scale
	byte		flags;			// Decal flags

	short		entityIndex;	// Entity this is attached to
};

typedef struct
{
	int			planenum;
	short		children[2];	// negative numbers are contents
} dclipnode_t;

typedef struct hull_s
{
	dclipnode_t* clipnodes;
	mplane_t* planes;
	int			firstclipnode;
	int			lastclipnode;
	vec3_t		clip_mins;
	vec3_t		clip_maxs;
} hull_t;

typedef struct msurface_s	msurface_t;

typedef struct cache_user_s
{
	void* data;
} cache_user_t;

// demand loaded sequence groups
typedef struct
{
	char				label[32];	// textual name
	char				name[64];	// file name
	cache_user_t		cache;		// cache index pointer
	int					data;		// hack for group 0
} mstudioseqgroup_t;

typedef struct model_s
{
	char		name[MAX_MODEL_NAME];
	qboolean	needload;		// bmodels and sprites don't cache normally

	modtype_t	type;
	int			numframes;
	synctype_t	synctype;

	int			flags;

	//
	// volume occupied by the model
	//		
	vec3_t		mins, maxs;
	float		radius;

	//
	// brush model
	//
	int			firstmodelsurface, nummodelsurfaces;

	int			numsubmodels;
	dmodel_t* submodels;

	int			numplanes;
	mplane_t* planes;

	int			numleafs;		// number of visible leafs, not counting 0
	struct mleaf_s* leafs;

	int			numvertexes;
	mvertex_t* vertexes;

	int			numedges;
	medge_t* edges;

	int			numnodes;
	mnode_t* nodes;

	int			numtexinfo;
	mtexinfo_t* texinfo;

	int			numsurfaces;
	msurface_t* surfaces;

	int			numsurfedges;
	int* surfedges;

	int			numclipnodes;
	dclipnode_t* clipnodes;

	int			nummarksurfaces;
	msurface_t** marksurfaces;

	hull_t		hulls[MAX_MAP_HULLS];

	int			numtextures;
	texture_t** textures;

	byte* visdata;

	color24* lightdata;

	char* entities;

	//
	// additional model data
	//
	cache_user_t	cache;		// only access through Mod_Extradata

} model_t;

typedef struct alight_s
{
	int			ambientlight;	// clip at 128
	int			shadelight;		// clip at 192 - ambientlight
	vec3_t		color;
	float* plightvec;
} alight_t;

// Rendering constants
enum
{
	kRenderNormal,			// src
	kRenderTransColor,		// c*a+dest*(1-a)
	kRenderTransTexture,	// src*a+dest*(1-a)
	kRenderGlow,			// src*a+dest -- No Z buffer checks
	kRenderTransAlpha,		// src*srca+dest*(1-srca)
	kRenderTransAdd,		// src*a+dest
};

enum
{
	kRenderFxNone = 0,
	kRenderFxPulseSlow,
	kRenderFxPulseFast,
	kRenderFxPulseSlowWide,
	kRenderFxPulseFastWide,
	kRenderFxFadeSlow,
	kRenderFxFadeFast,
	kRenderFxSolidSlow,
	kRenderFxSolidFast,
	kRenderFxStrobeSlow,
	kRenderFxStrobeFast,
	kRenderFxStrobeFaster,
	kRenderFxFlickerSlow,
	kRenderFxFlickerFast,
	kRenderFxNoDissipation,
	kRenderFxDistort,			// Distort/scale/translate flicker
	kRenderFxHologram,			// kRenderFxDistort + distance fade
	kRenderFxDeadPlayer,		// kRenderAmt is the player index
	kRenderFxExplode,			// Scale up really big!
	kRenderFxGlowShell,			// Glowing Shell
	kRenderFxClampMinScale,		// Keep this sprite from getting very small (SPRITES only!)
};

// bone controllers
typedef struct
{
	int					bone;	// -1 == 0
	int					type;	// X, Y, Z, XR, YR, ZR, M
	float				start;
	float				end;
	int					rest;	// byte index value at rest
	int					index;	// 0-3 user set controller, 4 mouth
} mstudiobonecontroller_t;

// animation frames
typedef union
{
	struct
	{
		byte	valid;
		byte	total;
	} num;
	short		value;
} mstudioanimvalue_t;

typedef struct engine_studio_api_s
{
	// Allocate number*size bytes and zero it
	void* (*Mem_Calloc)(int number, size_t size);

	// Check to see if pointer is in the cache
	void* (*Cache_Check)(struct cache_user_s* c);

	// Load file into cache ( can be swapped out on demand )
	void					(*LoadCacheFile)(char* path, struct cache_user_s* cu);

	// Retrieve model pointer for the named model
	struct model_s* (*Mod_ForName)(const char* name, int crash_if_missing);

	// Retrieve pointer to studio model data block from a model
	void* (*Mod_Extradata)(struct model_s* mod);

	// Retrieve indexed model from client side model precache list
	struct model_s* (*GetModelByIndex)(int index);

	// Get entity that is set for rendering
	struct cl_entity_s* (*GetCurrentEntity)(void);

	// Get referenced player_info_t
	struct player_info_s* (*PlayerInfo)(int index);

	// Get most recently received player state data from network system
	struct entity_state_s* (*GetPlayerState)(int index);

	// Get viewmodel entity
	struct cl_entity_s* (*GetViewEntity)(void);

	// Get current frame count, and last two timestampes on client
	void					(*GetTimes)(int* framecount, double* current, double* old);

	// Get a pointer to a cvar by name
	struct cvar_s* (*GetCvar)(const char* name);

	// Get current render origin and view vectors ( up, right and vpn )
	void					(*GetViewInfo)(float* origin, float* upv, float* rightv, float* vpnv);

	// Get sprite model used for applying chrome effect
	struct model_s* (*GetChromeSprite)(void);

	// Get model counters so we can incement instrumentation
	void					(*GetModelCounters)(int** s, int** a);

	// Get software scaling coefficients
	void					(*GetAliasScale)(float* x, float* y);

	// Get bone, light, alias, and rotation matrices
	float**** (*StudioGetBoneTransform)(void);
	float**** (*StudioGetLightTransform)(void);
	float*** (*StudioGetAliasTransform)(void);
	float*** (*StudioGetRotationMatrix)(void);

	// Set up body part, and get submodel pointers
	void					(*StudioSetupModel)(int bodypart, void** ppbodypart, void** ppsubmodel);

	// Check if entity's bbox is in the view frustum
	int						(*StudioCheckBBox)(void);

	// Apply lighting effects to model
	void					(*StudioDynamicLight)(struct cl_entity_s* ent, struct alight_s* plight);
	void					(*StudioEntityLight)(struct alight_s* plight);
	void					(*StudioSetupLighting)(struct alight_s* plighting);

	// Draw mesh vertices
	void					(*StudioDrawPoints)(void);

	// Draw hulls around bones
	void					(*StudioDrawHulls)(void);

	// Draw bbox around studio models
	void					(*StudioDrawAbsBBox)(void);

	// Draws bones
	void					(*StudioDrawBones)(void);

	// Loads in appropriate texture for model
	void					(*StudioSetupSkin)(void* ptexturehdr, int index);

	// Sets up for remapped colors
	void					(*StudioSetRemapColors)(int top, int bottom);

	// Set's player model and returns model pointer
	struct model_s* (*SetupPlayerModel)(int index);

	// Fires any events embedded in animation
	void					(*StudioClientEvents)(void);

	// Retrieve/set forced render effects flags
	int						(*GetForceFaceFlags)(void);
	void					(*SetForceFaceFlags)(int flags);

	// Tell engine the value of the studio model header
	void					(*StudioSetHeader)(void* header);

	// Tell engine which model_t * is being renderered
	void					(*SetRenderModel)(struct model_s* model);


	// Final state setup and restore for rendering
	void					(*SetupRenderer)(int rendermode);
	void					(*RestoreRenderer)(void);

	// Set render origin for applying chrome effect
	void					(*SetChromeOrigin)(void);

	// True if using D3D/OpenGL
	int						(*IsHardware)(void);

	// Only called by hardware interface
	void					(*GL_StudioDrawShadow)(void);
	void					(*GL_SetRenderMode)(int mode);

	void					(*StudioSetRenderamt)(int iRenderamt); 	//!!!CZERO added for rendering glass on viewmodels
	void					(*StudioSetCullState)(int iCull);
	void					(*StudioRenderShadow)(int iSprite, float* p1, float* p2, float* p3, float* p4);
};

#define	MAX_ALIAS_NAME	32

typedef struct cmdalias_s
{
	struct cmdalias_s* next;
	char	name[MAX_ALIAS_NAME];
	char* value;
} cmdalias_t;

typedef struct rect_s
{
	int				left, right, top, bottom;
} wrect_t;

typedef struct client_sprite_s
{
	char	szName[64];
	char	szSprite[64];
	int		hspr;
	int		iRes;
	wrect_t	rc;
} client_sprite_t;

typedef int HSPRITE_t;

enum
{
	SCRINFO_SCREENFLASH = 1,
	SCRINFO_STRETCHED
};

typedef struct SCREENINFO_s
{
	int		iSize;
	int		iWidth;
	int		iHeight;
	int		iFlags;
	int		iCharHeight;
	short	charWidths[256];
} SCREENINFO;

typedef struct hud_player_info_s
{
	// Name of the player
	char* name;

	short		ping;
	byte		thisplayer;  // TRUE if this is the calling player

	// stuff that's unused at the moment,  but should be done
	byte		spectator;
	byte		packetloss;

	// Return playermodel name
	char* model;

	short		topcolor;
	short		bottomcolor;

	unsigned __int64 m_nSteamID;

} hud_player_info_t;

typedef int (*pfnUserMsgHook)(const char* pszName, int iSize, void* pbuf);

typedef struct client_textmessage_s
{
	int			effect;
	byte		r1, g1, b1, a1;		// 2 colors for effects
	byte		r2, g2, b2, a2;
	float		x;
	float		y;
	float		fadein;
	float		fadeout;
	float		holdtime;
	float		fxtime;
	const char* pName;
	const char* pMessage;
} client_textmessage_t;

typedef struct cl_enginefuncs_s
{
	// sprite handlers
	HSPRITE_t(*pfnSPR_Load)(const char* szPicName);
	int							(*pfnSPR_Frames)(HSPRITE_t hPic);
	int							(*pfnSPR_Height)(HSPRITE_t hPic, int frame);
	int							(*pfnSPR_Width)(HSPRITE_t hPic, int frame);
	void						(*pfnSPR_Set)(HSPRITE_t hPic, int r, int g, int b);
	void						(*pfnSPR_Draw)(int frame, int x, int y, const wrect_t* prc);
	void						(*pfnSPR_DrawHoles)(int frame, int x, int y, const wrect_t* prc);
	void						(*pfnSPR_DrawAdditive)(int frame, int x, int y, const wrect_t* prc);
	void						(*pfnSPR_EnableScissor)	(int x, int y, int width, int height);
	void						(*pfnSPR_DisableScissor)(void);
	client_sprite_t* (*pfnSPR_GetList)(char* psz, int* piCount);

	// screen handlers
	void						(*pfnFillRGBA)(int x, int y, int width, int height, int r, int g, int b, int a);
	int							(*pfnGetScreenInfo)(SCREENINFO* pscrinfo);
	void						(*pfnSetCrosshair)(HSPRITE_t hspr, wrect_t rc, int r, int g, int b);

	// cvar handlers
	struct cvar_s* (*pfnRegisterVariable)(const char* szName, const char* szValue, int flags);
	float						(*pfnGetCvarFloat)(char* szName);
	char* (*pfnGetCvarString)	(const char* szName);

	// command handlers
	int							(*pfnAddCommand)(const char* cmd_name, void (*function)(void));
	int							(*pfnHookUserMsg)(char* szMsgName, pfnUserMsgHook pfn);
	int							(*pfnServerCmd)(char* szCmdString);
	int							(*pfnClientCmd)(const char* szCmdString);

	void						(*pfnGetPlayerInfo)(int ent_num, hud_player_info_t* pinfo);

	// sound handlers
	void						(*pfnPlaySoundByName)(char* szSound, float volume);
	void						(*pfnPlaySoundByIndex)(int iSound, float volume);

	// vector helpers
	void						(*pfnAngleVectors)(const float* vecAngles, float* forward, float* right, float* up);

	// text message system
	client_textmessage_t* (*pfnTextMessageGet)(const char* pName);
	int							(*pfnDrawCharacter)(int x, int y, int number, int r, int g, int b);
	int							(*pfnDrawConsoleString)(int x, int y, char* string);
	void						(*pfnDrawSetTextColor)(float r, float g, float b);
	void						(*pfnDrawConsoleStringLen)(const char* string, int* length, int* height);

	void						(*pfnConsolePrint)(const char* string);
	void						(*pfnCenterPrint)(const char* string);


	// Added for user input processing
	int							(*GetWindowCenterX)(void);
	int							(*GetWindowCenterY)(void);
	void						(*GetViewAngles)(float*);
	void						(*SetViewAngles)(float*);
	// Using GetMaxClients we can determine if client is connected
	// to the server at anytime, without using the pNetApi.
	int							(*GetMaxClients)(void);
	void						(*Cvar_SetValue)(const char* cvar, float value);

	int       					(*Cmd_Argc)(void);
	char* (*Cmd_Argv)(int arg);
	void						(*Con_Printf)(const char* fmt, ...);
	void						(*Con_DPrintf)(const char* fmt, ...);
	void						(*Con_NPrintf)(int pos, char* fmt, ...);
	void						(*Con_NXPrintf)(struct con_nprint_s* info, const char* fmt, ...);

	const char* (*PhysInfo_ValueForKey)	(const char* key);
	const char* (*ServerInfo_ValueForKey)(const char* key);
	float						(*GetClientMaxspeed)(void);
	int							(*CheckParm)(char* parm, char** ppnext);
	void						(*Key_Event)(int key, int down);
	void						(*GetMousePosition)(int* mx, int* my);
	int							(*IsNoClipping)(void);

	struct cl_entity_s* (*GetLocalPlayer)(void);
	struct cl_entity_s* (*GetViewModel)(void);
	struct cl_entity_s* (*GetEntityByIndex)(int idx);

	float						(*GetClientTime)(void);
	void						(*V_CalcShake)(void);
	void						(*V_ApplyShake)(float* origin, float* angles, float factor);

	int							(*PM_PointContents)(float* point, int* truecontents);
	int							(*PM_WaterEntity)(float* p);
	struct pmtrace_s* (*PM_TraceLine)(float* start, float* end, int flags, int usehull, int ignore_pe);

	struct model_s* (*CL_LoadModel)(const char* modelname, int* index);
	int							(*CL_CreateVisibleEntity)(int type, struct cl_entity_s* ent);

	const struct model_s* (*GetSpritePointer)(HSPRITE_t hSprite);
	void						(*pfnPlaySoundByNameAtLocation)	(char* szSound, float volume, float* origin);

	unsigned short				(*pfnPrecacheEvent)(int type, const char* psz);
	void						(*pfnPlaybackEvent)(int flags, const struct edict_s* pInvoker, unsigned short eventindex, float delay, float* origin, float* angles, float fparam1, float fparam2, int iparam1, int iparam2, int bparam1, int bparam2);
	void						(*pfnWeaponAnim)(int iAnim, int body);
	float						(*pfnRandomFloat)(float flLow, float flHigh);
	long						(*pfnRandomLong)(long lLow, long lHigh);
	void						(*pfnHookEvent)(char* name, void (*pfnEvent)(struct event_args_s* args));
	int							(*Con_IsVisible)();
	const char* (*pfnGetGameDirectory)(void);
	struct cvar_s* (*pfnGetCvarPointer)(const char* szName);
	const char* (*Key_LookupBinding)(const char* pBinding);
	const char* (*pfnGetLevelName)(void);
	void						(*pfnGetScreenFade)(struct screenfade_s* fade);
	void						(*pfnSetScreenFade)(struct screenfade_s* fade);
	void* (*VGui_GetPanel)();
	void						(*VGui_ViewportPaintBackground)(int extents[4]);

	byte* (*COM_LoadFile)(char* path, int usehunk, int* pLength);
	char* (*COM_ParseFile)(char* data, char* token);
	void						(*COM_FreeFile)(void* buffer);

	struct triangleapi_s* pTriAPI;
	struct efx_api_s* pEfxAPI;
	struct event_api_s* pEventAPI;
	struct demo_api_s* pDemoAPI;
	struct net_api_s* pNetAPI;
	struct IVoiceTweak_s* pVoiceTweak;

	// returns 1 if the client is a spectator only (connected to a proxy), 0 otherwise or 2 if in dev_overview mode
	int							(*IsSpectateOnly)(void);
	struct model_s* (*LoadMapSprite)(const char* filename);

	// file search functions
	void						(*COM_AddAppDirectoryToSearchPath)(const char* pszBaseDir, const char* appName);
	int							(*COM_ExpandFilename)(const char* fileName, char* nameOutBuffer, int nameOutBufferSize);

	// User info
	// playerNum is in the range (1, MaxClients)
	// returns NULL if player doesn't exit
	// returns "" if no value is set
	const char* (*PlayerInfo_ValueForKey)(int playerNum, const char* key);
	void						(*PlayerInfo_SetValueForKey)(const char* key, const char* value);

	// Gets a unique ID for the specified player. This is the same even if you see the player on a different server.
	// iPlayer is an entity index, so client 0 would use iPlayer=1.
	// Returns false if there is no player on the server in the specified slot.
	qboolean(*GetPlayerUniqueID)(int iPlayer, char playerID[16]);

	// TrackerID access
	int							(*GetTrackerIDForPlayer)(int playerSlot);
	int							(*GetPlayerForTrackerID)(int trackerID);

	// Same as pfnServerCmd, but the message goes in the unreliable stream so it can't clog the net stream
	// (but it might not get there).
	int							(*pfnServerCmdUnreliable)(char* szCmdString);

	void						(*pfnGetMousePos)(struct tagPOINT* ppt);
	void						(*pfnSetMousePos)(int x, int y);
	void						(*pfnSetMouseEnable)(qboolean fEnable);
	struct cvar_s* (*pfnGetCvarList)(void);
	struct cmd_s* (*pfnGetCmdList)(void);

	char* (*pfnGetCvarName)(struct cvar_s* cvar);
	char* (*pfnGetCmdName)(struct cmd_s* cmd);

	float						(*pfnGetServerTime)(void);
	float						(*pfnGetGravity)(void);
	const struct model_s* (*pfnPrecacheSprite)(HSPRITE_t spr);
	void						(*OverrideLightmap)(int override);
	void						(*SetLightmapColor)(float r, float g, float b);
	void						(*SetLightmapDarkness)(float dark);

	//this will always fail with the current engine
	int							(*pfnGetSequenceByName)(int flags, const char* seq);

	void 						(*pfnSPR_DrawGeneric)(int frame, int x, int y, const wrect_t* prc, int blendsrc, int blenddst, int unknown1, int unknown2);

	//this will always fail with engine, don't call
	//it actually has paramenters but i dunno what they do
	void 						(*pfnLoadSentence)(void);

	//localizes hud string, uses Legacy font from skin def
	// also supports unicode strings
	int 						(*pfnDrawLocalizedHudString)(int x, int y, const char* str, int r, int g, int b);

	//i can't get this to work for some reason, don't use this
	int 						(*pfnDrawLocalizedConsoleString)(int x, int y, const char* str);

	//gets keyvalue for local player, useful for querying vgui menus or autohelp
	const char* (*LocalPlayerInfo_ValueForKey)(const char* key);

	//another vgui2 text drawing function, i dunno how it works
	//it doesn't localize though
	void 						(*pfnDrawText_0)(int x, int y, const char* text, unsigned long font);

	int 						(*pfnDrawUnicodeCharacter)(int x, int y, short number, int r, int g, int b, unsigned long hfont);

	//checks sound header of a sound file, determines if its a supported type
	int 						(*pfnCheckSoundFile)(const char* path);

	//for condition zero, returns interface from GameUI
	void* (*GetCareerGameInterface)(void);

	void  						(*pfnCvar_Set)(const char* cvar, const char* value);

	//this actually checks for if the CareerGameInterface is found
	//and if a server is being run
	int  						(*IsSinglePlayer)(void);

	void  						(*pfnPlaySound)(const char* sound, float vol, float pitch);

	void  						(*pfnPlayMp3)(const char* mp3, int flags);

	//get the systems current time as a float
	float  						(*Sys_FloatTime)(void);

	void  						(*pfnSetArray)(int* array, int size);
	void  						(*pfnSetClearArray)(int* array, int size);
	void  						(*pfnClearArray)(void);

	void  						(*pfnPlaySound2)(const char* sound, float vol, float pitch);

	void	 					(*pfnTintRGBA)(int x, int y, int width, int height, int r, int g, int b, int a);

	int	 						(*pfnGetAppID)(void);
	cmdalias_t*			(*pfnGetAliasList)(void);
	void						(*pfnVguiWrap2_GetMouseDelta)(int* x, int* y);
} cl_enginefunc_t;

typedef struct client_data_s
{
	// fields that cannot be modified  (ie. have no effect if changed)
	vec3_t	origin;

	// fields that can be changed by the cldll
	vec3_t	viewangles;
	int		iWeaponBits;
	float	fov;	// field of view
} client_data_t;

// PM_PlayerTrace results.
typedef struct
{
	vec3_t	normal;
	float	dist;
} pmplane_t;

#define MAXSTUDIOCONTROLLERS 8

#define PM_NORMAL					0x00000000
#define PM_STUDIO_IGNORE			0x00000001 // Skip studio models
#define PM_STUDIO_BOX				0x00000002 // Use boxes for non-complex studio models (even in traceline)
#define PM_GLASS_IGNORE				0x00000004 // Ignore entities with non-normal rendermode
#define PM_WORLD_ONLY				0x00000008 // Only trace against the world

typedef struct pmtrace_s pmtrace_t;

struct pmtrace_s
{
	qboolean	allsolid;			// if true, plane is not valid
	qboolean	startsolid;			// if true, the initial point was in a solid area
	qboolean	inopen, inwater;	// End point is in empty space or in water
	float		fraction;			// time completed, 1.0 = didn't hit anything
	vec3_t		endpos;				// final position
	pmplane_t	plane;				// surface normal at impact
	int			ent;				// entity at impact
	vec3_t		deltavelocity;		// Change in player's velocity caused by impact.  
									// Only run on server.
	int			hitgroup;
};

typedef struct cl_clientfuncs_s
{
	int							(*Initialize)(cl_enginefunc_t* pEnginefuncs, int iVersion);
	int							(*HUD_Init)(void);
	int							(*HUD_VidInit)(void);
	void						(*HUD_Redraw)(float time, int intermission);
	int							(*HUD_UpdateClientData)(client_data_t* pcldata, float flTime);
	int							(*HUD_Reset)(void);
	void						(*HUD_PlayerMove)(struct playermove_s* pcl_playermove, int server);
	void						(*HUD_PlayerMoveInit)(struct playermove_s* pcl_playermove);
	char						(*HUD_PlayerMoveTexture)(char* name);
	void						(*IN_ActivateMouse)(void);
	void						(*IN_DeactivateMouse)(void);
	void						(*IN_MouseEvent)(int mstate);
	void						(*IN_ClearStates)(void);
	void						(*IN_Accumulate)(void);
	void						(*CL_CreateMove)(float frametime, struct usercmd_s* cmd, int active);
	int							(*CL_Isplayer_thirdperson)(void);
	void						(*CL_CameraOffset)(float* ofs);
	struct kbutton_s*			(*KB_Find)(const char* name);
	void						(*CAM_Think)(void);
	void						(*V_CalcRefdef)(struct ref_params_s* pparams);
	int							(*HUD_AddEntity)(int type, struct cl_entity_s* ent, const char* modelname);
	void						(*HUD_CreateEntities)(void);
	void						(*HUD_DrawNormalTriangles)(void);
	void						(*HUD_DrawTransparentTriangles)(void);
	void						(*HUD_StudioEvent)(const struct mstudioevent_s* event, const struct cl_entity_s* entity);
	void						(*HUD_PostRunCmd)(struct local_state_s* from, struct local_state_s* to, struct usercmd_s* cmd, int runfuncs, double time, unsigned int random_seed);
	void						(*HUD_Shutdown)(void);
	void						(*HUD_TxferLocalOverrides)(struct entity_state_s* state, const struct clientdata_s* client);
	void						(*HUD_ProcessPlayerState)(struct entity_state_s* dst, const struct entity_state_s* src);
	void						(*HUD_TxferPredictionData)(struct entity_state_s* ps, const struct entity_state_s* pps, struct clientdata_s* pcd, const struct clientdata_s* ppcd, struct weapon_data_s* wd, const struct weapon_data_s* pwd);
	void						(*Demo_ReadBuffer)(int size, unsigned char* buffer);
	int							(*HUD_ConnectionlessPacket)(struct netadr_s* net_from, const char* args, char* response_buffer, int* response_buffer_size);
	int							(*HUD_GetHullBounds)(int hullnumber, float* mins, float* maxs);
	void						(*HUD_Frame)(double time);
	int							(*HUD_Key_Event)(int down, int keynum, const char* pszCurrentBinding);
	void						(*HUD_TempEntUpdate)(double frametime, double client_time, double cl_gravity, struct tempent_s** ppTempEntFree, struct tempent_s** ppTempEntActive, int(*Callback_AddVisibleEntity)(struct cl_entity_s* pEntity), void(*Callback_TempEntPlaySound)(struct tempent_s* pTemp, float damp));
	struct cl_entity_s*			(*HUD_GetUserEntity)(int index);
	int							(*HUD_VoiceStatus)(int entindex, qboolean bTalking);
	int							(*HUD_DirectorMessage)(unsigned char command, unsigned int firstObject, unsigned int secondObject, unsigned int flags);
	int							(*HUD_GetStudioModelInterface)(int version, struct r_studio_interface_s** ppinterface, struct engine_studio_api_s* pstudio);
	void						(*HUD_CHATINPUTPOSITION_FUNCTION)(int* x, int* y);
	int							(*HUD_GETPLAYERTEAM_FUNCTION)(int iplayer);
	void						(*CLIENTFACTORY)(void);
} cl_clientfunc_t;

// lighting options
#define STUDIO_NF_FLATSHADE		0x0001
#define STUDIO_NF_CHROME		0x0002
#define STUDIO_NF_FULLBRIGHT	0x0004

// motion flags
#define STUDIO_X		0x0001
#define STUDIO_Y		0x0002	
#define STUDIO_Z		0x0004
#define STUDIO_XR		0x0008
#define STUDIO_YR		0x0010
#define STUDIO_ZR		0x0020
#define STUDIO_LX		0x0040
#define STUDIO_LY		0x0080
#define STUDIO_LZ		0x0100
#define STUDIO_AX		0x0200
#define STUDIO_AY		0x0400
#define STUDIO_AZ		0x0800
#define STUDIO_AXR		0x1000
#define STUDIO_AYR		0x2000
#define STUDIO_AZR		0x4000
#define STUDIO_TYPES	0x7FFF
#define STUDIO_RLOOP	0x8000	// controller that wraps shortest distance

#define STUDIO_RENDER 1
#define STUDIO_EVENTS 2

typedef enum
{
	TRI_FRONT = 0,
	TRI_NONE = 1,
} TRICULLSTYLE;

#define TRI_API_VERSION		1

#define TRI_TRIANGLES		0
#define TRI_TRIANGLE_FAN	1
#define TRI_QUADS			2
#define TRI_POLYGON			3
#define TRI_LINES			4	
#define TRI_TRIANGLE_STRIP	5
#define TRI_QUAD_STRIP		6
#define TRI_POINTS			7 // Xash3D added

typedef struct triangleapi_s
{
	int			version;

	void		(*RenderMode)(int mode);
	void		(*Begin)(int primitiveCode);
	void		(*End) (void);

	void		(*Color4f) (float r, float g, float b, float a);
	void		(*Color4ub) (unsigned char r, unsigned char g, unsigned char b, unsigned char a);
	void		(*TexCoord2f) (float u, float v);
	void		(*Vertex3fv) (float* worldPnt);
	void		(*Vertex3f) (float x, float y, float z);
	void		(*Brightness) (float brightness);
	void		(*CullFace) (TRICULLSTYLE style);
	int			(*SpriteTexture) (struct model_s* pSpriteModel, int frame);
	int			(*WorldToScreen) (float* world, float* screen);  // Returns 1 if it's z clipped
	void		(*Fog) (float flFogColor[3], float flStart, float flEnd, int bOn); //Works just like GL_FOG, flFogColor is r/g/b.
	void		(*ScreenToWorld) (float* screen, float* world);
	void	(*GetMatrix)(const int pname, float* matrix);
	int	(*BoxInPVS)(float* mins, float* maxs);
	void	(*LightAtPoint)(float* pos, float* value);
	void	(*Color4fRendermode)(float r, float g, float b, float a, int rendermode);
	void	(*FogParams)(float flDensity, int iFogSkybox);
} triangleapi_t;

#define EVENT_API_VERSION 1

typedef struct event_api_s
{
	int		version;
	void	(*EV_PlaySound) (int ent, float* origin, int channel, const char* sample, float volume, float attenuation, int fFlags, int pitch);
	void	(*EV_StopSound) (int ent, int channel, const char* sample);
	int		(*EV_FindModelIndex)(const char* pmodel);
	int		(*EV_IsLocal) (int playernum);
	int		(*EV_LocalPlayerDucking) (void);
	void	(*EV_LocalPlayerViewheight) (float*);
	void	(*EV_LocalPlayerBounds) (int hull, float* mins, float* maxs);
	int		(*EV_IndexFromTrace) (struct pmtrace_s* pTrace);
	struct physent_s* (*EV_GetPhysent) (int idx);
	void	(*EV_SetUpPlayerPrediction) (int dopred, int bIncludeLocalClient);
	void	(*EV_PushPMStates) (void);
	void	(*EV_PopPMStates) (void);
	void	(*EV_SetSolidPlayers) (int playernum);
	void	(*EV_SetTraceHull) (int hull);
	void	(*EV_PlayerTrace) (float* start, float* end, int traceFlags, int ignore_pe, struct pmtrace_s* tr);
	void	(*EV_WeaponAnimation) (int sequence, int body);
	unsigned short (*EV_PrecacheEvent) (int type, const char* psz);
	void	(*EV_PlaybackEvent) (int flags, const struct edict_s* pInvoker, unsigned short eventindex, float delay, float* origin, float* angles, float fparam1, float fparam2, int iparam1, int iparam2, int bparam1, int bparam2);
	const char* (*EV_TraceTexture) (int ground, float* vstart, float* vend);
	void	(*EV_StopAllSounds) (int entnum, int entchannel);
	void    (*EV_KillEvents) (int entnum, const char* eventname);
} event_api_t;

// bones
typedef struct
{
	char				name[32];	// bone name for symbolic links
	int		 			parent;		// parent bone
	int					flags;		// ??
	int					bonecontroller[6];	// bone controller index, -1 == none
	float				value[6];	// default DoF values
	float				scale[6];   // scale for delta DoF values
} mstudiobone_t;

// sequence descriptions
typedef struct
{
	char				label[32];	// sequence label

	float				fps;		// frames per second	
	int					flags;		// looping/non-looping flags

	int					activity;
	int					actweight;

	int					numevents;
	int					eventindex;

	int					numframes;	// number of frames per sequence

	int					numpivots;	// number of foot pivots
	int					pivotindex;

	int					motiontype;
	int					motionbone;
	vec3_t				linearmovement;
	int					automoveposindex;
	int					automoveangleindex;

	vec3_t				bbmin;		// per sequence bounding box
	vec3_t				bbmax;

	int					numblends;
	int					animindex;		// mstudioanim_t pointer relative to start of sequence group data
										// [blend][bone][X, Y, Z, XR, YR, ZR]

	int					blendtype[2];	// X, Y, Z, XR, YR, ZR
	float				blendstart[2];	// starting value
	float				blendend[2];	// ending value
	int					blendparent;

	int					seqgroup;		// sequence group for demand loading

	int					entrynode;		// transition node at entry
	int					exitnode;		// transition node at exit
	int					nodeflags;		// transition rules

	int					nextseq;		// auto advancing sequences
} mstudioseqdesc_t;

// attachment
typedef struct
{
	char				name[32];
	int					type;
	int					bone;
	vec3_t				org;	// attachment point
	vec3_t				vectors[3];
} mstudioattachment_t;

typedef struct
{
	unsigned short	offset[6];
} mstudioanim_t;

#include "studio/GameStudioModelRenderer.h"
#include "studio/studio_util.h"
#include "studio/StudioModelRenderer.h"

#pragma once
