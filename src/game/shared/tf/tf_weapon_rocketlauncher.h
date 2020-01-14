//====== Copyright � 1996-2005, Valve Corporation, All rights reserved. =======
//
// TF Rocket Launcher
//
//=============================================================================
#ifndef TF_WEAPON_ROCKETLAUNCHER_H
#define TF_WEAPON_ROCKETLAUNCHER_H
#ifdef _WIN32
#pragma once
#endif

#include "tf_weaponbase_gun.h"
#include "tf_weaponbase_rocket.h"


// Client specific.
#ifdef CLIENT_DLL

#define CTFRocketLauncher C_TFRocketLauncher
#define CTFOriginal C_TFOriginal
#define CTFSuperRocketLauncher C_TFSuperRocketLauncher
#define CTFCRPG C_TFCRPG
#define CTFCIncendiaryCannon C_TFCIncendiaryCannon
#else
#include "tf_projectile_rocket.h"
#endif

//=============================================================================
//
// TF Weapon Rocket Launcher.
//
class CTFRocketLauncher : public CTFWeaponBaseGun
{
public:

	DECLARE_CLASS( CTFRocketLauncher, CTFWeaponBaseGun );
	DECLARE_NETWORKCLASS(); 
	DECLARE_PREDICTABLE();

	// Server specific.
#ifdef GAME_DLL
	DECLARE_DATADESC();
#endif

	CTFRocketLauncher();
	~CTFRocketLauncher();

#ifndef CLIENT_DLL
	virtual void	Precache();
#endif
	virtual int		GetWeaponID( void ) const			{ return TF_WEAPON_ROCKETLAUNCHER; }
	virtual CBaseEntity *FireProjectile( CTFPlayer *pPlayer );
	virtual void	ItemPostFrame( void );
	virtual bool	Deploy( void );
	virtual bool	DefaultReload( int iClipSize1, int iClipSize2, int iActivity );

#ifdef CLIENT_DLL
	virtual void CreateMuzzleFlashEffects( C_BaseEntity *pAttachEnt, int nIndex );
	//virtual void DrawCrosshair( void );
#endif

	// List of active pipebombs
	typedef CHandle<CTFBaseRocket>	RocketHandle;
	CUtlVector<RocketHandle>		m_Rockets;	
	
	// This is here so we can network the pipebomb count for prediction purposes
	CNetworkVar( int,				m_iRocketCount );

private:
	float	m_flShowReloadHintAt;

	//CNetworkVar( bool, m_bLockedOn );

	CTFRocketLauncher( const CTFRocketLauncher & ) {}
};

// Mercenary specific version
class CTFOriginal : public CTFRocketLauncher
{
public:
	DECLARE_CLASS( CTFOriginal, CTFRocketLauncher );
	DECLARE_NETWORKCLASS(); 
	DECLARE_PREDICTABLE();

	virtual int		GetWeaponID( void ) const			{ return TF_WEAPON_ROCKETLAUNCHER_DM; }
};

//Quad RPG
class CTFSuperRocketLauncher : public CTFRocketLauncher
{
public:
	DECLARE_CLASS( CTFSuperRocketLauncher, CTFRocketLauncher );
	DECLARE_NETWORKCLASS(); 
	DECLARE_PREDICTABLE();

	CTFSuperRocketLauncher();
	
	virtual int		GetWeaponID( void ) const			{ return TF_WEAPON_SUPER_ROCKETLAUNCHER; }
	virtual	void	AddRocket( CTFBaseRocket *pRocket );
	virtual	void	SecondaryAttack( void );
	virtual	bool	DetonateRockets( void );
	virtual void	ItemPostFrame( void );
	virtual	void	DeathNotice( CBaseEntity *pVictim );
	virtual bool CanSoftZoom( void ) { return false; }

#ifdef CLIENT_DLL
	CNewParticleEffect *m_pEffect;
#endif

	float m_flLastPingSoundTime;
	
};

class CTFCRPG : public CTFRocketLauncher
{
public:
	DECLARE_CLASS( CTFCRPG, CTFRocketLauncher );
	DECLARE_NETWORKCLASS(); 
	DECLARE_PREDICTABLE();
	
#ifdef GAME_DLL
	DECLARE_DATADESC();
#endif

	CTFCRPG();
	
	virtual int		GetWeaponID( void ) const			{ return TFC_WEAPON_RPG; }
	
};

class CTFCIncendiaryCannon : public CTFRocketLauncher
{
public:
	DECLARE_CLASS( CTFCIncendiaryCannon, CTFRocketLauncher );
	DECLARE_NETWORKCLASS(); 
	DECLARE_PREDICTABLE();
	
#ifdef GAME_DLL
	DECLARE_DATADESC();
#endif

	CTFCIncendiaryCannon();
	
	virtual int		GetWeaponID( void ) const			{ return TFC_WEAPON_INCENDIARYCANNON; }
	
};


// Server specific
#ifdef GAME_DLL

//=============================================================================
//
// Generic rocket.
//
class CTFRocket : public CTFBaseRocket
{
public:

	DECLARE_CLASS( CTFRocket, CTFBaseRocket );

	// Creation.
	static CTFRocket *Create( const Vector &vecOrigin, const QAngle &vecAngles, CBaseEntity *pOwner = NULL );	
	virtual void Spawn();
	virtual void Precache();
};

#endif



#endif // TF_WEAPON_ROCKETLAUNCHER_H