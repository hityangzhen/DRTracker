#include "race/literace.h"
#include "core/log.h"

namespace race 
{

void LiteRace::Register()
{
	Detector::Register();
	knob_->RegisterBool("enable_literace",
		"whether enable the literace data race detector","0");
	knob_->RegisterInt("samp_type",
		"whether set the sample type (0-adaptive,1-fix)","0");
	knob_->RegisterInt("rate_param",
		"whether set the rate param","10");
	knob_->RegisterBool("track_racy_inst",
		"whether track potential racy instructions","0");

}

bool LiteRace::Enabled()
{
	return knob_->ValueBool("enable_literace");
}

void LiteRace::Setup(Mutex *lock,RaceDB *race_db)
{
	Djit::Setup(lock,race_db);
	samp_type_=(SampType)knob_->ValueInt("samp_type");
	rate_param_=knob_->ValueInt("rate_param");
}

Detector::Meta *LiteRace::GetMeta(address_t iaddr)
{
	Meta::Table::iterator it=meta_table_.find(iaddr);
	if(it==meta_table_.end()) {
		Meta *meta=new LrMeta(iaddr);
		meta_table_[iaddr]=meta;
		return meta;
	}
	return it->second;
}

void LiteRace::UpdateRate(LrMeta *meta)
{
	DEBUG_ASSERT(meta);
	if(samp_type_==ADAPTIVE) {
		meta->rate/=rate_param_;
		if(meta->rate<1)
			meta->rate=1;
	}
	meta->scount=meta->icount*meta->rate/1000;
	meta->ncount=meta->icount-meta->scount;
}

void LiteRace::ProcessRead(thread_t curr_thd_id,Meta *meta,Inst *inst)
{
	LrMeta *lr_meta=dynamic_cast<LrMeta *>(meta);
	DEBUG_ASSERT(lr_meta);
	lr_meta->icount++;
	if(lr_meta->ncount==0 && --lr_meta->scount>0)
		Djit::ProcessRead(curr_thd_id,meta,inst);
	else if(lr_meta->scount==0)
		UpdateRate(lr_meta);
	else
		lr_meta->ncount--;
}

void LiteRace::ProcessWrite(thread_t curr_thd_id,Meta *meta,Inst *inst)
{
	LrMeta *lr_meta=dynamic_cast<LrMeta *>(meta);
	DEBUG_ASSERT(lr_meta);
	lr_meta->icount++;
	if(lr_meta->ncount==0 && --lr_meta->scount>0)
		Djit::ProcessWrite(curr_thd_id,meta,inst);
	else if(lr_meta->scount==0)
		UpdateRate(lr_meta);
	else
		lr_meta->ncount--;
}

} //namespace race