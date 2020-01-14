#ifndef TF_BOT_USE_JUMP_PAD_H
#define TF_BOT_USE_JUMP_PAD_H
#ifdef _WIN32
#pragma once
#endif


#include "NextBotBehavior.h"

class CTFBotUseJumpPad : public Action<CTFBot>
{
public:
	CTFBotUseJumpPad();
	virtual ~CTFBotUseJumpPad();

	virtual const char *GetName() const override;

	virtual ActionResult<CTFBot> OnStart( CTFBot *me, Action<CTFBot> *priorAction ) override;
	virtual void OnEnd( CTFBot *me, Action<CTFBot> *action ) override;
	virtual ActionResult<CTFBot> OnSuspend( CTFBot *me, Action<CTFBot> *action ) override;
	virtual ActionResult<CTFBot> OnResume( CTFBot *me, Action<CTFBot> *action ) override;

	virtual ActionResult<CTFBot> Update( CTFBot *me, float dt ) override;

	virtual EventDesiredResult<CTFBot> OnContact( CTFBot *me, CBaseEntity *other, CGameTrace *trace ) override;
	virtual EventDesiredResult<CTFBot> OnMoveToSuccess( CTFBot *me, const Path *path ) override;
	virtual EventDesiredResult<CTFBot> OnMoveToFailure( CTFBot *me, const Path *path, MoveToFailureType reason ) override;
	virtual EventDesiredResult<CTFBot> OnStuck( CTFBot *me ) override;

	virtual QueryResultType ShouldHurry( const INextBot *me ) const override;

	static bool IsPossible( CTFBot *actor );

private:
	PathFollower m_PathFollower;
	CHandle<CBaseEntity> m_hJumpPad;
};

class CJumpPadFilter : public INextBotFilter
{
public:
	CJumpPadFilter( CTFPlayer *actor );

	virtual bool IsSelected( const CBaseEntity *ent ) const override;

private:
	CTFPlayer *m_pActor;
	float m_flMinCost;
};

#endif