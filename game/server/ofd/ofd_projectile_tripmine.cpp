//====== Copyright � 1996-2005, Valve Corporation, All rights reserved. =======
//
// Purpose: Moddified Tripmine entity for Open Fortress
//
//=============================================================================

#include "cbase.h"
#include "ofd_projectile_tripmine.h"
#include "tf_weaponbase_gun.h"
#include "tf_weaponbase_rocket.h"
#include "tf_gamerules.h"
#include "in_buttons.h"
#include "vstdlib/random.h"
#include "engine/IEngineSound.h"
#include "tf_player.h"
#include "basecombatcharacter.h"
#include "ai_basenpc.h"
#include "soundent.h"
#include "game.h"

//-----------------------------------------------------------------------------
// CTripmineGrenade
//-----------------------------------------------------------------------------

#define TRIPMINE_BEAM_SPRITE "sprites/laserbeam.vmt"
#define TRIPMINE_MODEL "models/weapons/w_models/w_tripmine_placed.mdl"


LINK_ENTITY_TO_CLASS( tf_projectile_tripmine, CTripmineGrenade );
PRECACHE_WEAPON_REGISTER( tf_projectile_tripmine );

BEGIN_DATADESC( CTripmineGrenade )
	DEFINE_FIELD( m_flPowerUp,	FIELD_TIME ),
	DEFINE_FIELD( m_vecDir,		FIELD_VECTOR ),
	DEFINE_FIELD( m_vecEnd,		FIELD_POSITION_VECTOR ),
	DEFINE_FIELD( m_flBeamLength, FIELD_FLOAT ),
//	DEFINE_FIELD( m_hBeam,		FIELD_EHANDLE ),
	DEFINE_FIELD( m_hRealOwner,	FIELD_EHANDLE ),
	DEFINE_FIELD( m_hStuckOn,	FIELD_EHANDLE ),
	DEFINE_FIELD( m_posStuckOn,	FIELD_POSITION_VECTOR ),
	DEFINE_FIELD( m_angStuckOn,	FIELD_VECTOR ),
	//DEFINE_FIELD( m_iLaserModel, FIELD_INTEGER ),

	// Function Pointers
	DEFINE_THINKFUNC( WarningThink ),
	DEFINE_THINKFUNC( PowerupThink ),
	DEFINE_THINKFUNC( BeamBreakThink ),
	DEFINE_THINKFUNC( DelayDeathThink ),
END_DATADESC()


CTripmineGrenade::CTripmineGrenade()
{
	m_vecDir.Init();
	m_vecEnd.Init();
}

void CTripmineGrenade::Spawn( void )
{
	Precache( );
	// motor
	SetMoveType( MOVETYPE_FLY );
	SetSolid( SOLID_BBOX );
	AddSolidFlags( FSOLID_NOT_SOLID );
	SetModel( TRIPMINE_MODEL );
	
	UTIL_SetSize( this, Vector( -8, -8, -8), Vector(8, 8, 8) );

	if ( m_spawnflags & 1 )
	{
		// power up quickly
		m_flPowerUp = gpGlobals->curtime + 1.0;
	}
	else
	{
		// power up in 2.5 seconds
		m_flPowerUp = gpGlobals->curtime + 2.5;
	}
	
	SetThink( &CTripmineGrenade::PowerupThink );
	SetNextThink( gpGlobals->curtime + 0.2 );

	m_takedamage = DAMAGE_YES;

	m_iHealth = 1;

	if ( GetOwnerEntity() != NULL )
	{
		// play deploy sound
		EmitSound( "Weapon_Tripmine.Deploy" );
		EmitSound( "Weapon_Tripmine.Charge" );

		m_hRealOwner = GetOwnerEntity();
	}
	AngleVectors( GetAbsAngles(), &m_vecDir );
	m_vecEnd = GetAbsOrigin() + m_vecDir * MAX_TRACE_LENGTH;
}


void CTripmineGrenade::Precache( void )
{
	PrecacheModel( TRIPMINE_MODEL ); 
	m_iLaserModel = PrecacheModel( TRIPMINE_BEAM_SPRITE );

	PrecacheScriptSound( "Weapon_Tripmine.Deploy" );
	PrecacheScriptSound( "Weapon_Tripmine.Charge" );
	PrecacheScriptSound( "Weapon_Tripmine.Activate" );

}


void CTripmineGrenade::WarningThink( void  )
{
	// set to power up
	SetThink( &CTripmineGrenade::PowerupThink );
	SetNextThink( gpGlobals->curtime + 1.0f );
}


void CTripmineGrenade::PowerupThink( void  )
{
	if ( m_hStuckOn == NULL )
	{
		trace_t tr;
		CBaseEntity *pOldOwner = GetOwnerEntity();

		// don't explode if the player is standing in front of the laser
		UTIL_TraceLine( GetAbsOrigin(), GetAbsOrigin() + m_vecDir * 32, MASK_SHOT, NULL, COLLISION_GROUP_NONE, &tr );

		if( tr.m_pEnt && pOldOwner &&
			( tr.m_pEnt == pOldOwner ) && pOldOwner->IsPlayer() )
		{
			m_flPowerUp += 0.1;	//delay the arming
			SetNextThink( gpGlobals->curtime + 0.1f );
			return;
		}

		// find out what we've been stuck on		
		SetOwnerEntity( NULL );
		
		UTIL_TraceLine( GetAbsOrigin() + m_vecDir * 8, GetAbsOrigin() - m_vecDir * 32, MASK_SHOT, pOldOwner, COLLISION_GROUP_NONE, &tr );

		if ( tr.startsolid )
		{
			SetOwnerEntity( pOldOwner );
			m_flPowerUp += 0.1;
			SetNextThink( gpGlobals->curtime + 0.1f );
			return;
		}
		if ( tr.fraction < 1.0 )
		{
			SetOwnerEntity( tr.m_pEnt );
			m_hStuckOn		= tr.m_pEnt;
			m_posStuckOn	= m_hStuckOn->GetAbsOrigin();
			m_angStuckOn	= m_hStuckOn->GetAbsAngles();
		}
		else
		{
			// somehow we've been deployed on nothing, or something that was there, but now isn't.
			// remove ourselves

			StopSound( "Weapon_Tripmine.Deploy" );
			StopSound( "Weapon_Tripmine.Charge" );
			SetThink( &CBaseEntity::SUB_Remove );
			SetNextThink( gpGlobals->curtime + 0.1f );
//			ALERT( at_console, "WARNING:Tripmine at %.0f, %.0f, %.0f removed\n", pev->origin.x, pev->origin.y, pev->origin.z );
			KillBeam();
			return;
		}
	}
	else if ( (m_posStuckOn != m_hStuckOn->GetAbsOrigin()) || (m_angStuckOn != m_hStuckOn->GetAbsAngles()) )
	{
		// what we were stuck on has moved, or rotated. Create a tripmine weapon and drop to ground

		StopSound( "Weapon_Tripmine.Deploy" );
		StopSound( "Weapon_Tripmine.Charge" );
		CBaseEntity *pMine = CBaseEntity::Create( "tf_projectile_tripmine", GetAbsOrigin() + m_vecDir * 24, GetAbsAngles() );
		pMine->AddSpawnFlags( SF_NORESPAWN );

		SetThink( &CBaseEntity::SUB_Remove );
		SetNextThink( gpGlobals->curtime + 0.1f );
		KillBeam();
		return;
	}

	if ( gpGlobals->curtime > m_flPowerUp )
	{
		MakeBeam( );
		RemoveSolidFlags( FSOLID_NOT_SOLID );

		m_bIsLive = true;

		// play enabled sound
		EmitSound( "Weapon_Tripmine.Activate" );
	}

	SetNextThink( gpGlobals->curtime + 0.1f );
}


void CTripmineGrenade::KillBeam( void )
{
	if ( m_hBeam )
	{
		UTIL_Remove( m_hBeam );
		m_hBeam = NULL;
	}
}


void CTripmineGrenade::MakeBeam( void )
{
	trace_t tr;

	UTIL_TraceLine( GetAbsOrigin(), m_vecEnd, MASK_SHOT, this, COLLISION_GROUP_NONE, &tr );

	m_flBeamLength = tr.fraction;

	// set to follow laser spot
	SetThink( &CTripmineGrenade::BeamBreakThink );

	SetNextThink( gpGlobals->curtime + 1.0f );

	Vector vecTmpEnd = GetAbsOrigin() + m_vecDir * MAX_TRACE_LENGTH * m_flBeamLength;

	m_hBeam = CBeam::BeamCreate( TRIPMINE_BEAM_SPRITE, 1.0 );
	m_hBeam->PointEntInit( vecTmpEnd, this );
	m_hBeam->SetColor( 255, 0, 0 );
	m_hBeam->SetScrollRate( 25.5 );
	m_hBeam->SetBrightness( 255 );
	m_hBeam->AddSpawnFlags( SF_BEAM_TEMPORARY );	// so it won't save and come back to haunt us later..
}


void CTripmineGrenade::BeamBreakThink( void  )
{
	bool bBlowup = false;
	trace_t tr;

	// NOT MASK_SHOT because we want only simple hit boxes
	UTIL_TraceLine( GetAbsOrigin(), m_vecEnd, MASK_SOLID, this, COLLISION_GROUP_NONE, &tr );

	// ALERT( at_console, "%f : %f\n", tr.flFraction, m_flBeamLength );

	// respawn detect. 
	if ( !m_hBeam )
	{
		MakeBeam();

		trace_t stuckOnTrace;
		Vector forward;
		GetVectors( &forward, NULL, NULL );

		UTIL_TraceLine( GetAbsOrigin(), GetAbsOrigin() - forward * 12.0f, MASK_SOLID, this, COLLISION_GROUP_NONE, &stuckOnTrace );

		if ( stuckOnTrace.m_pEnt )
		{
			m_hStuckOn = stuckOnTrace.m_pEnt;	// reset stuck on ent too
		}
	}

	CBaseEntity *pEntity = tr.m_pEnt;
	CBaseCombatCharacter *pBCC  = ToBaseCombatCharacter( pEntity );

	if ( pBCC || fabs( m_flBeamLength - tr.fraction ) > 0.001 )
	{
		bBlowup = true;
	}
	else
	{
		if ( m_hStuckOn == NULL )
			bBlowup = true;
		else if ( m_posStuckOn != m_hStuckOn->GetAbsOrigin() )
			bBlowup = true;
		else if ( m_angStuckOn != m_hStuckOn->GetAbsAngles() )
			bBlowup = true;
	}

	if ( bBlowup )
	{
		SetOwnerEntity( m_hRealOwner );
		m_iHealth = 0;
		Event_Killed( CTakeDamageInfo( this, m_hRealOwner, 100, GIB_NORMAL ) );
		return;
	}

	SetNextThink( gpGlobals->curtime + 0.1 );
}

int CTripmineGrenade::OnTakeDamage( const CTakeDamageInfo &info )
{
	if (gpGlobals->curtime < m_flPowerUp && info.GetDamage() < m_iHealth)
	{
		// disable
		// Create( "weapon_tripmine", GetLocalOrigin() + m_vecDir * 24, GetAngles() );
		SetThink( &CBaseEntity::SUB_Remove );
		SetNextThink( gpGlobals->curtime + 0.1f );
		KillBeam();
		return 0;
	}
	return BaseClass::OnTakeDamage( info );
}

void CTripmineGrenade::Event_Killed( const CTakeDamageInfo &info )
{
	m_takedamage = DAMAGE_NO;

	if ( info.GetAttacker() && ( info.GetAttacker()->GetFlags() & FL_CLIENT ) )
	{
		// some client has destroyed this mine, he'll get credit for any kills
		SetOwnerEntity( info.GetAttacker() );
	}

	SetThink( &CTripmineGrenade::DelayDeathThink );
	SetNextThink( gpGlobals->curtime + random->RandomFloat( 0.1, 0.3 ) );

	StopSound( "Weapon_Tripmine.Charge" );
}

void CTripmineGrenade::DelayDeathThink( void )
{
	KillBeam();
	trace_t tr;
	UTIL_TraceLine ( GetAbsOrigin() + m_vecDir * 8, GetAbsOrigin() - m_vecDir * 64,  MASK_SOLID, this, COLLISION_GROUP_NONE, & tr);

	Explode( &tr, DMG_BLAST );
}

//-----------------------------------------------------------------------------
// Purpose:
//-----------------------------------------------------------------------------
CTripmineGrenade *CTripmineGrenade::Create( CTFWeaponBase *pWeapon, const Vector &vecOrigin, const QAngle &vecAngles, CBaseEntity *pOwner, CBaseEntity *pScorer )
{
	CTripmineGrenade *pRocket = static_cast<CTripmineGrenade*>( CBaseEntity::Create( "tf_projectile_tripmine", vecOrigin, vecAngles, pOwner ) );

	return pRocket;
}

