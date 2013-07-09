/*
 *  cPopulationInterface.cc
 *  Avida
 *
 *  Called "pop_interface.cc" prior to 12/5/05.
 *  Copyright 1999-2011 Michigan State University. All rights reserved.
 *  Copyright 1993-2003 California Institute of Technology.
 *
 *
 *  This file is part of Avida.
 *
 *  Avida is free software; you can redistribute it and/or modify it under the terms of the GNU Lesser General Public License
 *  as published by the Free Software Foundation, either version 3 of the License, or (at your option) any later version.
 *
 *  Avida is distributed in the hope that it will be useful, but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU Lesser General Public License for more details.
 *
 *  You should have received a copy of the GNU Lesser General Public License along with Avida.
 *  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#include "cPopulationInterface.h"

#include "apto/platform.h"
#include "avida/core/Feedback.h"
#include "avida/systematics/Unit.h"

#include "cEnvironment.h"
#include "cHardwareManager.h"
#include "cOrganism.h"
#include "cOrgMessage.h"
#include "cPopulation.h"
#include "cStats.h"
#include "cTestCPU.h"
#include "cInstSet.h"

#include <cassert>
#include <algorithm>
#include <iterator>

#ifndef NULL
#define NULL 0
#endif

#if APTO_PLATFORM(WINDOWS)
namespace std
{
  /*inline __int64  abs(__int64 i) { return _abs64(i); }*/
}
#endif


cPopulationInterface::cPopulationInterface(cWorld* world)
: m_world(world)
, m_cell_id(-1)
, m_prevseen_cell_id(-1)
, m_prev_task_cell(-1)
, m_num_task_cells(0)
{
}

cPopulationInterface::~cPopulationInterface() {
}

cOrganism* cPopulationInterface::GetOrganism() {
  return GetCell()->GetOrganism();
}

const Apto::Array<cOrganism*, Apto::Smart>& cPopulationInterface::GetLiveOrgList() const {
  return m_world->GetPopulation().GetLiveOrgList();
}

cPopulationCell* cPopulationInterface::GetCell() { 
	return &m_world->GetPopulation().GetCell(m_cell_id);
}

cPopulationCell* cPopulationInterface::GetCell(int cell_id) { 
	return &m_world->GetPopulation().GetCell(cell_id);
}

int cPopulationInterface::GetCellXPosition()
{
  assert(false);
//  const int absolute_cell_ID = GetCellID();
//  std::pair<int, int> pos = m_world->GetPopulation().GetCellPosition(absolute_cell_ID);
//  return pos.first;
}

int cPopulationInterface::GetCellYPosition()
{
  assert(false);
//  const int absolute_cell_ID = GetCellID();
//  std::pair<int, int> pos = m_world->GetPopulation().GetCellPosition(absolute_cell_ID);
//  return pos.second;
}

cPopulationCell* cPopulationInterface::GetCellFaced() {
  return &GetCell()->GetCellFaced();
}

int cPopulationInterface::GetCellData() {
  m_world->GetPopulation().GetCell(m_cell_id).UpdateCellDataExpired();
  return m_world->GetPopulation().GetCell(m_cell_id).GetCellData();
}

int cPopulationInterface::GetCellDataOrgID() {
  m_world->GetPopulation().GetCell(m_cell_id).UpdateCellDataExpired();
  return m_world->GetPopulation().GetCell(m_cell_id).GetCellDataOrgID();
}

int cPopulationInterface::GetCellDataUpdate() {
  m_world->GetPopulation().GetCell(m_cell_id).UpdateCellDataExpired();
  return m_world->GetPopulation().GetCell(m_cell_id).GetCellDataUpdate();
}

int cPopulationInterface::GetCellDataTerritory() {
  m_world->GetPopulation().GetCell(m_cell_id).UpdateCellDataExpired();
  return m_world->GetPopulation().GetCell(m_cell_id).GetCellDataTerritory();
}

int cPopulationInterface::GetCellDataForagerType() {
  m_world->GetPopulation().GetCell(m_cell_id).UpdateCellDataExpired();
  return m_world->GetPopulation().GetCell(m_cell_id).GetCellDataForagerType();
}

int cPopulationInterface::GetFacedCellData() {
  return m_world->GetPopulation().GetCell(m_cell_id).GetCellFaced().GetCellData();
}

int cPopulationInterface::GetFacedCellDataOrgID() {
  return m_world->GetPopulation().GetCell(m_cell_id).GetCellFaced().GetCellDataOrgID();
}

int cPopulationInterface::GetFacedCellDataUpdate() {
  return m_world->GetPopulation().GetCell(m_cell_id).GetCellFaced().GetCellDataUpdate();
}

int cPopulationInterface::GetFacedCellDataTerritory() {
  return m_world->GetPopulation().GetCell(m_cell_id).GetCellFaced().GetCellDataTerritory();
}

void cPopulationInterface::SetCellData(const int newData) {
  cPopulationCell& cell = m_world->GetPopulation().GetCell(m_cell_id);
  cell.SetCellData(newData, cell.GetOrganism()->GetID());
}

bool cPopulationInterface::GetLGTFragment(cAvidaContext& ctx, int region, const Genome& dest_genome, InstructionSequence& seq)
{
  const int MAX_POP_SAMPLES = 10;
  ConstInstructionSequencePtr src_seq(NULL);
  
  switch (region) {
      // Local Neighborhood
    case 0:
    {
      Apto::Array<cPopulationCell*> occupied_cells;
      GetCell()->GetOccupiedNeighboringCells(occupied_cells);
      
      int num_cells = occupied_cells.GetSize();
      for (int i = 0; i < num_cells;) {
        const Genome* cell_genome = &occupied_cells[i]->GetOrganism()->GetGenome();
        if (cell_genome->HardwareType() != dest_genome.HardwareType()) {
          // Organism type mis-match, remove from consideration;
          num_cells--;
          occupied_cells[i] = occupied_cells[num_cells];
        } else {
          i++;
        }
      }
      
      if (num_cells == 0) return false;
      
      int cell_idx = ctx.GetRandom().GetInt(num_cells);
      src_seq.DynamicCastFrom(occupied_cells[cell_idx]->GetOrganism()->GetGenome().Representation());
    }
      break;
      
      // Entire Population
    case 1:
    {
      const Apto::Array<cOrganism*, Apto::Smart>& live_org_list = m_world->GetPopulation().GetLiveOrgList();
      for (int i = 0; i < MAX_POP_SAMPLES; i++) {
        int org_idx = ctx.GetRandom().GetInt(live_org_list.GetSize());
        const Genome* org_genome = &live_org_list[org_idx]->GetGenome();
        if (org_genome->HardwareType() != dest_genome.HardwareType()) {
          src_seq.DynamicCastFrom(org_genome->Representation());
          break;
        }
      }
      
      if (!src_seq) return false;
    }
      break;
      
    default:
      return false;
  }
  
  assert(src_seq);
  
  // Select random start and end point
  int from = ctx.GetRandom().GetInt(src_seq->GetSize());
  int to = ctx.GetRandom().GetInt(src_seq->GetSize());

  // Order from and to indices
  if (from > to) {
    int tmp = to;
    to = from;
    from = tmp;
  }
  
  // Resize outgoing sequence and copy over the fragment
  int new_size = to - from;
  if (new_size == 0) {
    // zero size treated as transfer of the whole genome
    seq = (*src_seq);
  } else {
    seq.Resize(new_size);
    for (int i = from; i < to; i++) {
      seq[i - from] = (*src_seq)[i];
    }
  }
  
  return true;
}


bool cPopulationInterface::Divide(cAvidaContext& ctx, cOrganism* parent, const Genome& offspring_genome)
{
  assert(parent != NULL);
  assert(m_world->GetPopulation().GetCell(m_cell_id).GetOrganism() == parent);
  return m_world->GetPopulation().ActivateOffspring(ctx, offspring_genome, parent);
}

cOrganism* cPopulationInterface::GetNeighbor()
{
  cPopulationCell& cell = m_world->GetPopulation().GetCell(m_cell_id);
  assert(cell.IsOccupied());
  
  return cell.ConnectionList().GetFirst()->GetOrganism();
}

bool cPopulationInterface::IsNeighborCellOccupied() {
  cPopulationCell & cell = m_world->GetPopulation().GetCell(m_cell_id);
  return cell.ConnectionList().GetFirst()->IsOccupied();
}

int cPopulationInterface::GetNumNeighbors()
{
  cPopulationCell & cell = m_world->GetPopulation().GetCell(m_cell_id);
  assert(cell.IsOccupied());
  
  return cell.ConnectionList().GetSize();
}

void cPopulationInterface::GetNeighborhoodCellIDs(Apto::Array<int>& list)
{
  cPopulationCell& cell = m_world->GetPopulation().GetCell(m_cell_id);
  assert(cell.IsOccupied());
  
  list.Resize(cell.ConnectionList().GetSize());
  tConstListIterator<cPopulationCell> it(cell.ConnectionList());
  int i = 0;
  while (it.Next() != NULL) list[i++] = it.Get()->GetID();
}

void cPopulationInterface::GetAVNeighborhoodCellIDs(Apto::Array<int>& list, int av_num)
{
  cPopulationCell& cell = m_world->GetPopulation().GetCell(m_avatars[av_num].av_cell_id);
  assert(cell.HasAV());
  
  list.Resize(cell.ConnectionList().GetSize());
  tConstListIterator<cPopulationCell> it(cell.ConnectionList());
  int i = 0;
  while (it.Next() != NULL) list[i++] = it.Get()->GetID();
}

int cPopulationInterface::GetFacing()
{
	cPopulationCell& cell = m_world->GetPopulation().GetCell(m_cell_id);
	assert(cell.IsOccupied());
	return cell.GetFacing();
}

int cPopulationInterface::GetFacedCellID()
{
	cPopulationCell& cell = m_world->GetPopulation().GetCell(m_cell_id).GetCellFaced();
	return cell.GetID();
}

int cPopulationInterface::GetFacedDir()
{
	cPopulationCell& cell = m_world->GetPopulation().GetCell(m_cell_id);
	assert(cell.IsOccupied());
	return cell.GetFacedDir();
}

int cPopulationInterface::GetNeighborCellContents() {
  cPopulationCell & cell = m_world->GetPopulation().GetCell(m_cell_id);
  return cell.ConnectionList().GetFirst()->GetCellData();
}

void cPopulationInterface::Rotate(cAvidaContext& ctx, int direction)
{
  cPopulationCell & cell = m_world->GetPopulation().GetCell(m_cell_id);
  assert(cell.IsOccupied());
	
  if (m_world->GetConfig().USE_AVATARS.Get()) {
    if (direction >= 0) RotateAV(ctx, 1);
    else RotateAV(ctx, -1);
  }
  else {
    if (direction >= 0) cell.ConnectionList().CircNext();
    else cell.ConnectionList().CircPrev();
  }
}

int cPopulationInterface::GetInputAt(int& input_pointer)
{
  cPopulationCell& cell = m_world->GetPopulation().GetCell(m_cell_id);
  //assert(cell.IsOccupied());
  return cell.GetInputAt(input_pointer);
}

void cPopulationInterface::ResetInputs(cAvidaContext& ctx) 
{ 
  m_world->GetPopulation().GetCell(m_cell_id).ResetInputs(ctx); 
}

const Apto::Array<int>& cPopulationInterface::GetInputs() const
{
  return m_world->GetPopulation().GetCell(m_cell_id).GetInputs();
}

const Apto::Array<double>& cPopulationInterface::GetResources(cAvidaContext& ctx)
{
  return m_world->GetPopulation().GetResources().GetCellResources(m_cell_id, ctx); 
}

double cPopulationInterface::GetResourceVal(cAvidaContext& ctx, int res_id)
{
  return m_world->GetPopulation().GetResources().GetCellResVal(ctx, m_cell_id, res_id);
}

const Apto::Array<double>& cPopulationInterface::GetFacedCellResources(cAvidaContext& ctx)
{
  return m_world->GetPopulation().GetResources().GetCellResources(GetCell()->GetCellFaced().GetID(), ctx); 
}

double cPopulationInterface::GetFacedResourceVal(cAvidaContext& ctx, int res_id)
{
  return m_world->GetPopulation().GetResources().GetCellResVal(ctx, GetCell()->GetCellFaced().GetID(), res_id);
}

const Apto::Array<double>& cPopulationInterface::GetCellResources(int cell_id, cAvidaContext& ctx)
{
  return m_world->GetPopulation().GetResources().GetCellResources(cell_id, ctx); 
}

const Apto::Array<double>& cPopulationInterface::GetFrozenResources(cAvidaContext& ctx, int cell_id)
{
  return m_world->GetPopulation().GetResources().GetFrozenResources(ctx, cell_id); 
}

double cPopulationInterface::GetFrozenCellResVal(cAvidaContext& ctx, int cell_id, int res_id)
{
  return m_world->GetPopulation().GetResources().GetFrozenCellResVal(ctx, cell_id, res_id);
}

double cPopulationInterface::GetCellResVal(cAvidaContext& ctx, int cell_id, int res_id)
{
  return m_world->GetPopulation().GetResources().GetCellResVal(ctx, cell_id, res_id);
}

const Apto::Array< Apto::Array<int> >& cPopulationInterface::GetCellIdLists()
{
	return m_world->GetPopulation().GetResources().GetCellIdLists();
}

int cPopulationInterface::GetCurrPeakX(cAvidaContext& ctx, int res_id) 
{ 
  return m_world->GetPopulation().GetResources().GetCurrPeakX(ctx, res_id); 
} 

int cPopulationInterface::GetCurrPeakY(cAvidaContext& ctx, int res_id) 
{ 
  return m_world->GetPopulation().GetResources().GetCurrPeakY(ctx, res_id); 
} 

int cPopulationInterface::GetFrozenPeakX(cAvidaContext& ctx, int res_id) 
{ 
  return m_world->GetPopulation().GetResources().GetFrozenPeakX(ctx, res_id); 
} 

int cPopulationInterface::GetFrozenPeakY(cAvidaContext& ctx, int res_id) 
{ 
  return m_world->GetPopulation().GetResources().GetFrozenPeakY(ctx, res_id); 
} 

void cPopulationInterface::TriggerDoUpdates(cAvidaContext& ctx)
{
  m_world->GetPopulation().GetResources().TriggerDoUpdates(ctx);
}

void cPopulationInterface::UpdateResources(cAvidaContext& ctx, const Apto::Array<double>& res_change)
{
  return m_world->GetPopulation().GetResources().UpdateCellResources(ctx, res_change, m_cell_id);
}

void cPopulationInterface::Die(cAvidaContext& ctx)
{
  cPopulationCell & cell = m_world->GetPopulation().GetCell(m_cell_id);
  m_world->GetPopulation().KillOrganism(cell, ctx);
}

void cPopulationInterface::KillOrganism(cAvidaContext& ctx, int cell_id)
{
  cPopulationCell & cell = m_world->GetPopulation().GetCell(cell_id);
  m_world->GetPopulation().KillOrganism(cell, ctx);
}


void cPopulationInterface::KillCellID(int target, cAvidaContext& ctx)
{
  cPopulationCell & cell = m_world->GetPopulation().GetCell(target);
  m_world->GetPopulation().KillOrganism(cell, ctx); 
}

void cPopulationInterface::Kaboom(int distance, cAvidaContext& ctx) 
{
  cPopulationCell & cell = m_world->GetPopulation().GetCell(m_cell_id);
  m_world->GetPopulation().Kaboom(cell, ctx, distance); 
}

int cPopulationInterface::ReceiveValue()
{
  cPopulationCell & cell = m_world->GetPopulation().GetCell(m_cell_id);
  assert(cell.IsOccupied());
  
  const int num_neighbors = cell.ConnectionList().GetSize();
  for (int i = 0; i < num_neighbors; i++) {
    cell.ConnectionList().CircNext();
    
    cOrganism* cur_neighbor = cell.ConnectionList().GetFirst()->GetOrganism();
    if (cur_neighbor == NULL || cur_neighbor->GetSentActive() == false) {
      continue;
    }
    
    return cur_neighbor->RetrieveSentValue();
  }
  
  return 0;
}

bool cPopulationInterface::InjectParasite(cOrganism* host, Systematics::UnitPtr parent, const cString& label, const InstructionSequence& injected_code)
{
  assert(parent != NULL);
  assert(m_world->GetPopulation().GetCell(m_cell_id).GetOrganism() == host);
  
  return m_world->GetPopulation().ActivateParasite(host, parent, label, injected_code);
}

bool cPopulationInterface::UpdateMerit(double new_merit)
{
  return m_world->GetPopulation().UpdateMerit(m_cell_id, new_merit);
}

bool cPopulationInterface::TestOnDivide()
{
  return m_world->GetTestOnDivide();
}




int cPopulationInterface::GetStateGridID(cAvidaContext& ctx)
{
  return ctx.GetRandom().GetUInt(m_world->GetEnvironment().GetNumStateGrids());
}

/* Rotate an organism to face the neighbor with the highest reputation */
void cPopulationInterface::RotateToGreatestReputation() 
{
	
	cPopulationCell& cell = m_world->GetPopulation().GetCell(GetCellID());
	int high_rep=-1; 
	vector <int> high_rep_orgs;
	
	
	// loop to find the max reputation
	for(int i=0; i<cell.ConnectionList().GetSize(); ++i) {
		const cPopulationCell* faced_cell = cell.ConnectionList().GetFirst();
		// cell->organism, if occupied, check reputation, etc.
		if (IsNeighborCellOccupied()) {
			cOrganism* cur_neighbor = faced_cell->GetOrganism();
			
			// if it has high reputation	
			if (cur_neighbor->GetReputation() >= high_rep) {
				if (cur_neighbor->GetReputation() > high_rep) {
					high_rep = cur_neighbor->GetReputation();
					high_rep_orgs.clear();
				}
				high_rep_orgs.push_back(cur_neighbor->GetID()); 
			} 
		}
		
		// check the next neighbor
		cell.ConnectionList().CircNext();
	}
	
	// Pick an organism to donate to
	
	if (high_rep_orgs.size() > 0) {
		unsigned int rand_num = m_world->GetRandom().GetUInt(0, high_rep_orgs.size()); 
		int high_org_id = high_rep_orgs[rand_num];
		
		for(int i=0; i<cell.ConnectionList().GetSize(); ++i) {
			const cPopulationCell* faced_cell = cell.ConnectionList().GetFirst();
			
			if (IsNeighborCellOccupied()) {
				
				cOrganism* cur_neighbor = faced_cell->GetOrganism();
				
				// if it has high reputation	
				if (cur_neighbor->GetID() == high_org_id) {
					break;
				}
			}
			
			cell.ConnectionList().CircNext();
			
		}
	}
	
}

/* Rotate an organism to face the neighbor with the highest reputation 
 where the neighbor has a different tag than the organism*/
void cPopulationInterface::RotateToGreatestReputationWithDifferentTag(int tag) 
{
	
	cPopulationCell& cell = m_world->GetPopulation().GetCell(GetCellID());
	int high_rep=-1; 
	vector <int> high_rep_orgs;
	
	// loop to find the max reputation
	for(int i=0; i<cell.ConnectionList().GetSize(); ++i) {
		const cPopulationCell* faced_cell = cell.ConnectionList().GetFirst();
		// cell->organism, if occupied, check reputation, etc.
		if (IsNeighborCellOccupied()) {
			cOrganism* cur_neighbor = faced_cell->GetOrganism();
			
			// if it has high reputation	
			if ((cur_neighbor->GetTagLabel() != tag) && (cur_neighbor->GetReputation() >= high_rep)) {
				if (cur_neighbor->GetReputation() > high_rep) {
					high_rep = cur_neighbor->GetReputation();
					high_rep_orgs.clear();
				}
				high_rep_orgs.push_back(cur_neighbor->GetID()); 
			} 
		}
		
		// check the next neighbor
		cell.ConnectionList().CircNext();
	}
	
	// Pick an organism to donate to
	
	if (high_rep_orgs.size() > 0) {
		unsigned int rand_num = m_world->GetRandom().GetUInt(0, high_rep_orgs.size()); 
		int high_org_id = high_rep_orgs[rand_num];
		
		for(int i=0; i<cell.ConnectionList().GetSize(); ++i) {
			const cPopulationCell* faced_cell = cell.ConnectionList().GetFirst();
			
			if (IsNeighborCellOccupied()) {
				
				cOrganism* cur_neighbor = faced_cell->GetOrganism();
				
				// if it has high reputation	
				if (cur_neighbor->GetID() == high_org_id) {
					break;
				}
			}
			
			cell.ConnectionList().CircNext();
			
		}
		
		
		
	}
	
}

/* Rotate an organism to face the neighbor with the highest reputation 
 where the neighbor has a different tag than the organism*/
void cPopulationInterface::RotateToGreatestReputationWithDifferentLineage(int line) 
{
	
	cPopulationCell& cell = m_world->GetPopulation().GetCell(GetCellID());
	int high_rep=-1; 
	vector <int> high_rep_orgs;
	
	// loop to find the max reputation
	for(int i=0; i<cell.ConnectionList().GetSize(); ++i) {
		const cPopulationCell* faced_cell = cell.ConnectionList().GetFirst();
		// cell->organism, if occupied, check reputation, etc.
		if (IsNeighborCellOccupied()) {
			cOrganism* cur_neighbor = faced_cell->GetOrganism();
			
			// if it has high reputation	
			if ((cur_neighbor->GetLineageLabel() != line) && (cur_neighbor->GetReputation() >= high_rep)) {
				if (cur_neighbor->GetReputation() > high_rep) {
					high_rep = cur_neighbor->GetReputation();
					high_rep_orgs.clear();
				}
				high_rep_orgs.push_back(cur_neighbor->GetID()); 
			} 
		}
		
		// check the next neighbor
		cell.ConnectionList().CircNext();
	}
	
	// Pick an organism to donate to
	
	if (high_rep_orgs.size() > 0) {
		unsigned int rand_num = m_world->GetRandom().GetUInt(0, high_rep_orgs.size()); 
		int high_org_id = high_rep_orgs[rand_num];
		
		for(int i=0; i<cell.ConnectionList().GetSize(); ++i) {
			const cPopulationCell* faced_cell = cell.ConnectionList().GetFirst();
			
			if (IsNeighborCellOccupied()) {
				
				cOrganism* cur_neighbor = faced_cell->GetOrganism();
				
				// if it has high reputation	
				if (cur_neighbor->GetID() == high_org_id) {
					break;
				}
			}
			
			cell.ConnectionList().CircNext();
			
		}
	}	
}


bool cPopulationInterface::Move(cAvidaContext& ctx, int src_id, int dest_id)
{
  return m_world->GetPopulation().MoveOrganisms(ctx, src_id, dest_id, -1);
}

void cPopulationInterface::AddLiveOrg()  
{
  m_world->GetPopulation().AddLiveOrg(GetOrganism());
}

void cPopulationInterface::RemoveLiveOrg() 
{
  m_world->GetPopulation().RemoveLiveOrg(GetOrganism());
}

bool cPopulationInterface::HasOpinion(cOrganism* in_organism)
{
  return in_organism->HasOpinion();
}

void cPopulationInterface::SetOpinion(int opinion, cOrganism* in_organism)
{
  in_organism->SetOpinion(opinion);
}

void cPopulationInterface::ClearOpinion(cOrganism* in_organism)
{
  in_organism->ClearOpinion();
}

void cPopulationInterface::JoinGroup(int group_id)
{
  m_world->GetPopulation().JoinGroup(GetOrganism(), group_id);
}

void cPopulationInterface::MakeGroup()
{
  m_world->GetPopulation().MakeGroup(GetOrganism());
}

void cPopulationInterface::LeaveGroup(int group_id)
{
  m_world->GetPopulation().LeaveGroup(GetOrganism(), group_id);
}

int cPopulationInterface::NumberOfOrganismsInGroup(int group_id)
{
  return m_world->GetPopulation().NumberOfOrganismsInGroup(group_id);
}

int cPopulationInterface::NumberGroupFemales(int group_id)
{
  return m_world->GetPopulation().NumberGroupFemales(group_id);
}

int cPopulationInterface::NumberGroupMales(int group_id)
{
  return m_world->GetPopulation().NumberGroupMales(group_id);
}

int cPopulationInterface::NumberGroupJuvs(int group_id)
{
  return m_world->GetPopulation().NumberGroupJuvs(group_id);
}

void cPopulationInterface::ChangeGroupMatingTypes(cOrganism* org, int group_id, int old_type, int new_type)  
{
  m_world->GetPopulation().ChangeGroupMatingTypes(org, group_id, old_type, new_type);  
}

/* Increases tolerance towards the addition of members to the group.
 * toleranceType:
 *    0: increases tolerance towards immigrants
 *    1: increases tolerance towards own offspring
 *    2: increases tolerance towards other offspring of the group
 * Removes the most recent record of dec-tolerance
 * Returns the modified tolerance total.
 */
int cPopulationInterface::IncTolerance(const int tolerance_type, cAvidaContext &ctx)
{
  int group_id = GetOrganism()->GetOpinion().first;
  
  if (tolerance_type == 0) {
    // Modify tolerance towards immigrants
    PushToleranceInstExe(0, ctx);
    
    // Update tolerance list by removing the most recent dec_tolerance record
    delete GetOrganism()->GetPhenotype().GetToleranceImmigrants().Pop();
    
    // If not at individual's max tolerance, adjust both caches
    if (GetOrganism()->GetPhenotype().GetIntolerances()[0].second != 0) {
      GetOrganism()->GetPhenotype().GetIntolerances()[0].second--;
      GetGroupIntolerances(group_id, 0, -1)--;
      if (m_world->GetConfig().TOLERANCE_VARIATIONS.Get() == 2) {
        if (GetOrganism()->GetPhenotype().GetMatingType() == MATING_TYPE_FEMALE) GetGroupIntolerances(group_id, 0, 0)--;
        else if (GetOrganism()->GetPhenotype().GetMatingType() == MATING_TYPE_MALE) GetGroupIntolerances(group_id, 0, 1)--;
        else if (GetOrganism()->GetPhenotype().GetMatingType() == MATING_TYPE_JUVENILE) GetGroupIntolerances(group_id, 0, 2)--;
      }
    }
    // Retrieve modified tolerance total for immigrants
    return GetOrganism()->GetPhenotype().CalcToleranceImmigrants();
  }
  if (tolerance_type == 1) {
    // Modify tolerance towards own offspring
    PushToleranceInstExe(1, ctx);
    
    // Update tolerance list by removing the most recent dec_tolerance record
    delete  GetOrganism()->GetPhenotype().GetToleranceOffspringOwn().Pop();
    
    // If not at max tolerance, decrease the intolerance cache
    if (GetOrganism()->GetPhenotype().GetIntolerances()[1].second != 0) {
      GetOrganism()->GetPhenotype().GetIntolerances()[1].second--;
    }
    // Retrieve modified tolerance total for own offspring.
    return GetOrganism()->GetPhenotype().CalcToleranceOffspringOwn();
  }
  if (tolerance_type == 2) {
    // Modify tolerance towards other offspring of the group
    PushToleranceInstExe(2, ctx);
    
    // Update tolerance list by removing the most recent dec_tolerance record
    delete GetOrganism()->GetPhenotype().GetToleranceOffspringOthers().Pop();
    
    // If not at max tolerance, decrease the intolerance cache
    if (GetOrganism()->GetPhenotype().GetIntolerances()[2].second != 0) {
      GetOrganism()->GetPhenotype().GetIntolerances()[2].second--;
      GetGroupIntolerances(group_id, 1, -1)--;
    }
    // Retrieve modified tolerance total for other offspring in group.
    return GetOrganism()->GetPhenotype().CalcToleranceOffspringOthers();
  }
  return -1;
}

/* Decreases tolerance towards the addition of members to the group.
 * toleranceType:
 *    0: decreases tolerance towards immigrants
 *    1: decreases tolerance towards own offspring
 *    2: decreases tolerance towards other offspring of the group
 * Records the update during which dec-tolerance was executed
 * Returns the modified tolerance total.
 */
int cPopulationInterface::DecTolerance(const int tolerance_type, cAvidaContext &ctx)
{
  const int cur_update = m_world->GetStats().GetUpdate();
  const int tolerance_max = m_world->GetConfig().MAX_TOLERANCE.Get();
  int group_id = GetOrganism()->GetOpinion().first;
  
  if (tolerance_type == 0) {
    // Modify tolerance towards immigrants
    PushToleranceInstExe(3, ctx);
    
    // Update tolerance list by inserting new record (at the front)
    tList<int>& tolerance_list = GetOrganism()->GetPhenotype().GetToleranceImmigrants();
    tolerance_list.Push(new int(cur_update));
    if (tolerance_list.GetSize() > tolerance_max) delete tolerance_list.PopRear();
    
    // If not at min tolerance, increase the intolerance cache
    if (GetOrganism()->GetPhenotype().GetIntolerances()[0].second != tolerance_max) {
      GetOrganism()->GetPhenotype().GetIntolerances()[0].second++;
      GetGroupIntolerances(group_id, 0, -1)++;
      if (m_world->GetConfig().TOLERANCE_VARIATIONS.Get() == 2) {
        if (GetOrganism()->GetPhenotype().GetMatingType() == MATING_TYPE_FEMALE) GetGroupIntolerances(group_id, 0, 0)++;
        else if (GetOrganism()->GetPhenotype().GetMatingType() == MATING_TYPE_MALE) GetGroupIntolerances(group_id, 0, 1)++;
        else if (GetOrganism()->GetPhenotype().GetMatingType() == MATING_TYPE_JUVENILE) GetGroupIntolerances(group_id, 0, 2)++;
      }
    }
    
    // Return modified tolerance total for immigrants.
    return GetOrganism()->GetPhenotype().CalcToleranceImmigrants();
  }
  if (tolerance_type == 1) {
    PushToleranceInstExe(4, ctx);
    
    // Update tolerance list by inserting new record (at the front)
    tList<int>& tolerance_list = GetOrganism()->GetPhenotype().GetToleranceOffspringOwn();
    tolerance_list.Push(new int(cur_update));
    if(tolerance_list.GetSize() > tolerance_max) delete tolerance_list.PopRear();
    
    // If not at min tolerance, increase the intolerance cache
    if (GetOrganism()->GetPhenotype().GetIntolerances()[1].second != tolerance_max) {
      GetOrganism()->GetPhenotype().GetIntolerances()[1].second++;
    }
    // Return modified tolerance total for own offspring.
    return GetOrganism()->GetPhenotype().CalcToleranceOffspringOwn();

  }
  if (tolerance_type == 2) {
    PushToleranceInstExe(5, ctx);
    
    // Update tolerance list by inserting new record (at the front)
    tList<int>& tolerance_list = GetOrganism()->GetPhenotype().GetToleranceOffspringOthers();
    tolerance_list.Push(new int(cur_update));
    if(tolerance_list.GetSize() > tolerance_max) delete tolerance_list.PopRear();
    
    // If not at min tolerance, increase the intolerance cache
    if (GetOrganism()->GetPhenotype().GetIntolerances()[2].second != tolerance_max) {
      GetOrganism()->GetPhenotype().GetIntolerances()[2].second++;
      GetOrganism()->GetOrgInterface().GetGroupIntolerances(group_id, 1, -1)++;
    }
    // Retrieve modified tolerance total for other offspring in the group.
    return GetOrganism()->GetPhenotype().CalcToleranceOffspringOthers();
  }
  return -1;
}

int cPopulationInterface::CalcGroupToleranceImmigrants(int prop_group_id, int mating_type)
{
  return m_world->GetPopulation().CalcGroupToleranceImmigrants(prop_group_id, mating_type);
}

int cPopulationInterface::CalcGroupToleranceOffspring(cOrganism* parent_organism)
{
  return m_world->GetPopulation().CalcGroupToleranceOffspring(parent_organism);
}

double cPopulationInterface::CalcGroupOddsImmigrants(int group_id, int mating_type)
{
  return m_world->GetPopulation().CalcGroupOddsImmigrants(group_id, mating_type);
}

double cPopulationInterface::CalcGroupOddsOffspring(cOrganism* parent)
{
  return m_world->GetPopulation().CalcGroupOddsOffspring(parent);
}

double cPopulationInterface::CalcGroupOddsOffspring(int group_id)
{
  return m_world->GetPopulation().CalcGroupOddsOffspring(group_id);
}

bool cPopulationInterface::AttemptImmigrateGroup(cAvidaContext& ctx, int group_id, cOrganism* org)
{
  return m_world->GetPopulation().AttemptImmigrateGroup(ctx, group_id, org);
}

void cPopulationInterface::PushToleranceInstExe(int tol_inst, cAvidaContext& ctx)
{
  if(!m_world->GetConfig().TRACK_TOLERANCE.Get()) {
    m_world->GetStats().PushToleranceInstExe(tol_inst);
    return;
  }
  
  const Apto::Array<double> res_count = GetResources(ctx);
  
  int group_id = GetOrganism()->GetOpinion().first;
  int group_size = NumberOfOrganismsInGroup(group_id);
  double resource_level = res_count[group_id];
  int tol_max = m_world->GetConfig().MAX_TOLERANCE.Get();
  
  double immigrant_odds = CalcGroupOddsImmigrants(group_id, -1);
  double offspring_own_odds;
  double offspring_others_odds;
  int tol_immi = GetOrganism()->GetPhenotype().CalcToleranceImmigrants();
  int tol_own;
  int tol_others;
  
  if(m_world->GetConfig().TOLERANCE_VARIATIONS.Get() > 0) {
    offspring_own_odds = 1.0;
    offspring_others_odds = 1.0;
    tol_own = tol_max;
    tol_others = tol_max;
  } else {
    offspring_own_odds = CalcGroupOddsOffspring(GetOrganism());
    offspring_others_odds = CalcGroupOddsOffspring(group_id);
    tol_own = GetOrganism()->GetPhenotype().CalcToleranceOffspringOwn();
    tol_others = GetOrganism()->GetPhenotype().CalcToleranceOffspringOthers();
  }
  
  double odds_immi = immigrant_odds * 100;
  double odds_own = offspring_own_odds * 100;
  double odds_others = offspring_others_odds * 100;
  
  m_world->GetStats().PushToleranceInstExe(tol_inst, group_id, group_size, resource_level, odds_immi, odds_own, odds_others, tol_immi, tol_own, tol_others, tol_max);
  return;
}

int& cPopulationInterface::GetGroupIntolerances(int group_id, int tol_num, int mating_type)
{
  return m_world->GetPopulation().GetGroupIntolerances(group_id, tol_num, mating_type);
}

void cPopulationInterface::TryWriteGroupAttackBits(unsigned char raw_bits)
{
  m_world->GetStats().PrintGroupAttackBits(raw_bits);
}

void cPopulationInterface::TryWriteGroupAttackString(cString& string)
{
  m_world->GetStats().PrintGroupAttackString(string);
}

void cPopulationInterface::DecNumPreyOrganisms()
{
  m_world->GetPopulation().DecNumPreyOrganisms();
}

void cPopulationInterface::DecNumPredOrganisms()
{
  m_world->GetPopulation().DecNumPredOrganisms();
}

void cPopulationInterface::DecNumTopPredOrganisms()
{
  m_world->GetPopulation().DecNumTopPredOrganisms();
}

void cPopulationInterface::IncNumPreyOrganisms()
{
  m_world->GetPopulation().IncNumPreyOrganisms();
}

void cPopulationInterface::IncNumPredOrganisms()
{
  m_world->GetPopulation().IncNumPredOrganisms();
}

void cPopulationInterface::IncNumTopPredOrganisms()
{
  m_world->GetPopulation().IncNumTopPredOrganisms();
}

void cPopulationInterface::AttackFacedOrg(cAvidaContext& ctx, int loser)
{
  m_world->GetPopulation().AttackFacedOrg(ctx, loser);
}

void cPopulationInterface::TryWriteBirthLocData(int org_idx)
{
  if (m_world->GetConfig().TRACK_BIRTH_LOCS.Get()) m_world->GetStats().PrintBirthLocData(org_idx);
}

void cPopulationInterface::InjectPreyClone(cAvidaContext& ctx, int gen_id)
{
  cOrganism* org_to_clone = NULL;
  const Apto::Array<cOrganism*, Apto::Smart>& live_org_list = GetLiveOrgList();
  Apto::Array<cOrganism*> TriedIdx(live_org_list.GetSize());
  int list_size = TriedIdx.GetSize();
  for (int i = 0; i < list_size; i ++) { TriedIdx[i] = live_org_list[i]; }
  
  int idx = ctx.GetRandom().GetUInt(list_size);
  while (org_to_clone == NULL) {
    cOrganism* org_at = TriedIdx[idx];
    // exclude pred and juvs & the genotype of the org that was being killed and triggered this cloning
    if (org_at->GetForageTarget() > -1 && org_at->SystematicsGroup("genotype")->ID() != gen_id &&
      !org_at->GetPhenotype().IsClone()) org_to_clone = org_at;   // only clone orgs that can reproduce on their own
    else TriedIdx.Swap(idx, --list_size);
    if (list_size == 1) break;
    idx = ctx.GetRandom().GetUInt(list_size);
  }
  if (org_to_clone != NULL) m_world->GetPopulation().InjectPreyClone(ctx, org_to_clone);
}

void cPopulationInterface::KillRandPred(cAvidaContext& ctx, cOrganism* org)
{
  m_world->GetPopulation().KillRandPred(ctx, org);
}

void cPopulationInterface::KillRandPrey(cAvidaContext& ctx, cOrganism* org)
{
  m_world->GetPopulation().KillRandPrey(ctx, org);
}

void cPopulationInterface::TryWriteLookData(cString& string)
{
  if (m_world->GetConfig().TRACK_LOOK_SETTINGS.Get()) m_world->GetStats().PrintLookData(string);
}

void cPopulationInterface::TryWriteLookOutput(cString& string)
{
  if (m_world->GetConfig().TRACK_LOOK_OUTPUT.Get()) m_world->GetStats().PrintLookDataOutput(string);
}

void cPopulationInterface::TryWriteLookEXOutput(cString& string)
{
  if (m_world->GetConfig().TRACK_LOOK_OUTPUT.Get()) m_world->GetStats().PrintLookEXDataOutput(string);
}

// -------- Avatar support --------
/* Each organism carries an array of avatars linking the organism to any cells it is occupying.
 * Each cell contains an array of the organisms with avatars in that cell, linking the cells back to
 * the organisms (in cPopulationCell). This allows both multiple organisms to occupy the same cell
 * and organisms to occupy/interact with multiple cells. Currently only two types of avatars are
 * supported: input and output, also used as predators and prey. 
 */

// Check if the avatar has any output avatars sharing the same cell
bool cPopulationInterface::HasOutputAV(int av_num)
{
  // If the avatar exists..
  if (av_num < GetNumAV()) {
    // Check the avatar's cell for an output avatar
    return m_world->GetPopulation().GetCell(m_avatars[av_num].av_cell_id).HasOutputAV(GetOrganism());
  }
  return false;
}

// Check if the avatar's faced cell has any output avatars
bool cPopulationInterface::FacedHasOutputAV(int av_num)
{
  // If the avatar exists..
  if (av_num < GetNumAV()) {
    // Check the avatar's faced cell for an output avatar
    return m_world->GetPopulation().GetCell(m_avatars[av_num].av_faced_cell).HasOutputAV(GetOrganism());
  }
  return false;
}

// Check if the avatar's faced cell has any avatars
bool cPopulationInterface::FacedHasAV(int av_num)
{
  // If the avatar exists..
  if (av_num < GetNumAV()) {
    // Check the avatar's faced cell for other avatars
    return m_world->GetPopulation().GetCell(m_avatars[av_num].av_faced_cell).HasAV();
  }
  return false;
}

// Check if the avatar's faced cell has any predator (input) avatars
bool cPopulationInterface::FacedHasPredAV(int av_num)
{
  // If the avatar exists..
  if (av_num < GetNumAV()) {
    // Check the faced cell for predators (inputs)
    return m_world->GetPopulation().GetCell(m_avatars[av_num].av_faced_cell).HasInputAV();
  }
  return false;
}

// Check it the avatar's faced cell has any prey (output) avatars
bool cPopulationInterface::FacedHasPreyAV(int av_num)
{
  // If the avatar exists..
  if (av_num < GetNumAV()) {
    // Check the faced cell for prey (outputs)
    return m_world->GetPopulation().GetCell(m_avatars[av_num].av_faced_cell).HasOutputAV();
  }
  return false;
}

// Creates a new avatar and adds it to the cell avatar lists
void cPopulationInterface::AddIOAV(cAvidaContext& ctx, int av_cell_id, int av_facing, bool input, bool output)
{
  // Add new avatar to m_avatars
  sIO_avatar tmpAV(av_cell_id, av_facing, -1, input, output);
  m_avatars.Push(tmpAV);

  // If this is an input avatar add to the target cell
  if (input) {
    m_world->GetPopulation().GetCell(av_cell_id).AddPredAV(ctx, GetOrganism());
  }

  // If this is an output avatar add to the target cell
  if (output) {
    m_world->GetPopulation().GetCell(av_cell_id).AddPreyAV(ctx, GetOrganism());
  }

  // Find the created avatar's faced cell
  SetAVFacedCellID(ctx, GetNumAV() - 1);
}

// Creates a new avatar based on the organism's forage target as a predator or prey, and adds it to the cell
void cPopulationInterface::AddPredPreyAV(cAvidaContext& ctx, int av_cell_id)
{
  // Add predator (saved as input avatar)
  if (!GetOrganism()->IsPreyFT()) {
    sIO_avatar predAV(av_cell_id, 0, -1, true, false);
    m_avatars.Push(predAV);
    m_world->GetPopulation().GetCell(av_cell_id).AddPredAV(ctx, GetOrganism());
  // Add prey (saved as output avatar)
  } else {
    sIO_avatar preyAV(av_cell_id, 0, -1, false, true);
    m_avatars.Push(preyAV);
    m_world->GetPopulation().GetCell(av_cell_id).AddPreyAV(ctx, GetOrganism());
  }
  // Find the created avatar's faced cell
  SetAVFacedCellID(ctx, GetNumAV() - 1);
}

// Switches the avatar from being a predator to a prey avatar or vice-versa
void cPopulationInterface::SwitchPredPrey(cAvidaContext& ctx, int av_num)
{
  // If the avatar exists..
  if (av_num < GetNumAV()) {
    // Is a predator, switching to a prey (input to output)
    if (m_avatars[av_num].av_input) {
      m_world->GetPopulation().GetCell(m_avatars[av_num].av_cell_id).RemovePredAV(GetOrganism());
      m_world->GetPopulation().GetCell(m_avatars[av_num].av_cell_id).AddPreyAV(ctx, GetOrganism());
      m_avatars[av_num].av_input = false;
      m_avatars[av_num].av_output = true;
    // Is prey, switching to a predator (output to intput)
    } else if (m_avatars[av_num].av_output) {
      m_world->GetPopulation().GetCell(m_avatars[av_num].av_cell_id).RemovePreyAV(GetOrganism());
      m_world->GetPopulation().GetCell(m_avatars[av_num].av_cell_id).AddPredAV(ctx, GetOrganism());
      m_avatars[av_num].av_input = true;
      m_avatars[av_num].av_output = false;
    }
  }
}

// Removes all the organism's avatars and removes them from cell lists
void cPopulationInterface::RemoveAllAV()
{
  // Cycle through removing all avatars
  for (int i = 0; i < GetNumAV(); i++) {
    sIO_avatar tmpAV = m_avatars.Pop();
    // Check that avatar is actually in a cell
    if (tmpAV.av_cell_id >= 0) {
      // If input avatar remove from the cell
      if (tmpAV.av_input) {
        m_world->GetPopulation().GetCell(tmpAV.av_cell_id).RemovePredAV(GetOrganism());
      }
      // If output avatar remove from the cell
      if (tmpAV.av_output) {
        m_world->GetPopulation().GetCell(tmpAV.av_cell_id).RemovePreyAV(GetOrganism());
      }
    }
  }
}

// Returns the avatar's faced direction
int cPopulationInterface::GetAVFacing(int av_num)
{
  // If the avatar exists..
  if (av_num < GetNumAV()) {
    // Return avatar's facing
    return m_avatars[av_num].av_facing;
  }
  return 0;
}

// Returns the avatar's cell id
int cPopulationInterface::GetAVCellID(int av_num)
{
  // If the avatar exists..
  if (av_num < GetNumAV()) {
    // Return the avatar's cell id
    return m_avatars[av_num].av_cell_id;
  }
  return -1;
}

// Returns cell id faced by avatar, only for torus and bounded worlds
int cPopulationInterface::GetAVFacedCellID(int av_num)//** GetCellXPosition()
{
  if (av_num < GetNumAV()) {
    return m_avatars[av_num].av_faced_cell;
  }
  return -1;
}

// Returns the number of cells neighboring the avatar's
// Avatar facing and movement only works in bounded or toroidial geometries
int cPopulationInterface::GetAVNumNeighbors(int av_num)
{
  if ((m_world->GetConfig().WORLD_GEOMETRY.Get() != 1) && (m_world->GetConfig().WORLD_GEOMETRY.Get() != 2)) {
    m_world->GetDriver().Feedback().Error("Not valid WORLD_GEOMETRY for USE_AVATAR, must be torus or bounded.");
    m_world->GetDriver().Abort(INVALID_CONFIG);
  }

  // If the avatar exists..
  if (av_num < GetNumAV()) {
    if (m_world->GetConfig().WORLD_GEOMETRY.Get() == 2) return 8;

    const int cell_id = m_avatars[av_num].av_cell_id;
    const int x_size = m_world->GetConfig().WORLD_X.Get();
    const int y_size = m_world->GetConfig().WORLD_Y.Get();
    int x = cell_id % x_size;
    int y = cell_id / x_size;

    // Is the cell on a corner..
    if (x == 0 || x == (x_size - 1)) {
      if (y == 0 || y == (y_size - 1)) return 3;
      // Is the cell on a side-edge..
      else return 5;
    }
    // Is the cell on a top or bottom edge..
    if (y == 0 || y == (y_size - 1)) return 5;
    // The cell must be on the interior..
    return 8;
  }
  return 0;
}

// Returns the avatar's faced cell data
int cPopulationInterface::GetAVFacedData(int av_num)
{
  // If the avatar exists..
  if (av_num < GetNumAV()) {
    // Return the avatar's faced cell data
    return m_world->GetPopulation().GetCell(m_avatars[av_num].av_faced_cell).GetCellData();
  }
  return -1;
}

// Returns the avatar's faced cell org id data
int cPopulationInterface::GetAVFacedDataOrgID(int av_num)
{
  // If the avatar exists..
  if (av_num < GetNumAV()) {
    // Returns the avatar's faced cell org id data
    return m_world->GetPopulation().GetCell(m_avatars[av_num].av_faced_cell).GetCellDataOrgID();
  }
  return -1;
}

// Returns the avatar's faced cell update data
int cPopulationInterface::GetAVFacedDataUpdate(int av_num)
{
  // If the avatar exists..
  if (av_num < GetNumAV()) {
    // Returns the avatar's faced cell update data
    return m_world->GetPopulation().GetCell(m_avatars[av_num].av_faced_cell).GetCellDataUpdate();
  }
  return -1;
}

// Returns the avatar's faced cell territory data
int cPopulationInterface::GetAVFacedDataTerritory(int av_num)
{
  // If the avatar exists..
  if (av_num < GetNumAV()) {
    // Returns the avatar's faced cell territory data
    return m_world->GetPopulation().GetCell(m_avatars[av_num].av_faced_cell).GetCellDataTerritory();
  }
  return -1;
}

// Returns the avatar's faced cell data
int cPopulationInterface::GetAVData(int av_num)
{
  // If the avatar exists..
  if (av_num < GetNumAV()) {
    // Return the avatar's faced cell data
    return m_world->GetPopulation().GetCell(m_avatars[av_num].av_cell_id).GetCellData();
  }
  return -1;
}

// Returns the avatar's faced cell org id data
int cPopulationInterface::GetAVDataOrgID(int av_num)
{
  // If the avatar exists..
  if (av_num < GetNumAV()) {
    // Returns the avatar's faced cell org id data
    return m_world->GetPopulation().GetCell(m_avatars[av_num].av_cell_id).GetCellDataOrgID();
  }
  return -1;
}

// Returns the avatar's faced cell update data
int cPopulationInterface::GetAVDataUpdate(int av_num)
{
  // If the avatar exists..
  if (av_num < GetNumAV()) {
    // Returns the avatar's faced cell update data
    return m_world->GetPopulation().GetCell(m_avatars[av_num].av_cell_id).GetCellDataUpdate();
  }
  return -1;
}

// Returns the avatar's faced cell territory data
int cPopulationInterface::GetAVDataTerritory(int av_num)
{
  // If the avatar exists..
  if (av_num < GetNumAV()) {
    // Returns the avatar's faced cell territory data
    return m_world->GetPopulation().GetCell(m_avatars[av_num].av_cell_id).GetCellDataTerritory();
  }
  return -1;
}

// Finds the index of the next avatar which matches input/output specifications
int cPopulationInterface::FindAV(bool input, bool output, int av_num)
{
  assert(GetNumAV() > 0);
  const int num_AV = GetNumAV();
  for (int i = 0; i < num_AV; i++) {
    int index = i + av_num % num_AV;
    if (m_avatars[index].av_input == input && m_avatars[index].av_output == output) {
      return index;
    }
  }
  return -1;
}

// Sets the avatar's facing, then sets the faced cell
void cPopulationInterface::SetAVFacing(cAvidaContext& ctx, int av_facing, int av_num)
{
  // If the avatar exists..
  if (av_num < GetNumAV()) {
    m_avatars[av_num].av_facing = av_facing;
    // Set the new avatar faced cell id
    SetAVFacedCellID(ctx, av_num);
  }
}

// Changes the avatar's cell id, moving it to the correct cell lists, and find the new faced cell id
bool cPopulationInterface::SetAVCellID(cAvidaContext& ctx, int av_cell_id, int av_num)
{
  // If the avatar exists..
  if (av_num < GetNumAV()) {
    // Not necessary to move the avatar
    if (m_avatars[av_num].av_cell_id == av_cell_id) return false;

    // If the avatar was previously in another cell remove it
    if (m_avatars[av_num].av_cell_id > -1) {
      if (m_avatars[av_num].av_input) {
        m_world->GetPopulation().GetCell(m_avatars[av_num].av_cell_id).RemovePredAV(GetOrganism());
      }
      if (m_avatars[av_num].av_output) {
        m_world->GetPopulation().GetCell(m_avatars[av_num].av_cell_id).RemovePreyAV(GetOrganism());
      }
    }

    // If it is an input avatar, add to the new cell
    if (m_avatars[av_num].av_input) {
      m_world->GetPopulation().GetCell(av_cell_id).AddPredAV(ctx, GetOrganism());
    }
    // If it is an output avatar, add to the new cell
    if (m_avatars[av_num].av_output) {
      m_world->GetPopulation().GetCell(av_cell_id).AddPreyAV(ctx, GetOrganism());
    }

    // Set the avatar's cell
    m_avatars[av_num].av_cell_id = av_cell_id;

    // Set the avatar's new faced cell
    SetAVFacedCellID(ctx, av_num);
    return true;
  }
  return false;
}

// Determine and store the cell id faced by the avatar
// Note:
void cPopulationInterface::SetAVFacedCellID(cAvidaContext& ctx, int av_num)
{
  const int world_geometry = m_world->GetConfig().WORLD_GEOMETRY.Get();
  // Avatars only supported in bounded and toroidal world geometries
  if ((world_geometry != 1) && (world_geometry != 2)) {
    m_world->GetDriver().Feedback().Error("Not valid WORLD_GEOMETRY for USE_AVATAR, must be torus or bounded.");
    m_world->GetDriver().Abort(INVALID_CONFIG);
  }

  // If the avatar exists..
  if (av_num < GetNumAV()) {
    const int x_size = m_world->GetConfig().WORLD_X.Get();
    const int y_size = m_world->GetConfig().WORLD_Y.Get();

    const int old_cell_id = m_avatars[av_num].av_cell_id;
    const int facing = m_avatars[av_num].av_facing;

    int x = old_cell_id % x_size;
    int y = old_cell_id / x_size;


    bool off_the_edge_facing = false;
    // If a bounded grid, do checks for facing off the edge of a bounded world grid..
    if (world_geometry == 1) {
      // Check if the avatar is at the end of a single column world
      if (x_size == 1) {
        if (y == 0) {
          y += 1;
          off_the_edge_facing = true;
        } 
        else if (y == (y_size - 1)) {
          y -= 1;
          off_the_edge_facing = true;
        }
      // Check if the avatar is at the end of a single row world
      } else if (y_size == 1) {
        if (x == 0) {
          x += 1;
          off_the_edge_facing = true;
        } 
        else if (y == (y_size - 1)) {
          x -= 1;
          off_the_edge_facing = true;
        }
      }

      // The world is neither a single row or column, continuing border facing checks
      if (!off_the_edge_facing) {
        // West edge..
        if (x == 0) {
          // Northwest corner
          if (y == 0) {
            if (facing == 0 || facing == 7 || facing == 6) {
              if (ctx.GetRandom().GetInt(0, 2)) x += 1;
              else y += 1;
              off_the_edge_facing = true;
            }
            else if (facing == 5) {
              y += 1;
              off_the_edge_facing = true;
            }
            else if (facing == 1) {
              x += 1;
              off_the_edge_facing = true;
            }
          }
          // Southwest corner
          else if (y == (y_size - 1)) {
            if (facing == 4 || facing == 5 || facing == 6) {
              if (ctx.GetRandom().GetInt(0, 2)) x += 1;
              else y -= 1;
              off_the_edge_facing = true;
            }
            else if (facing == 7) {
              x += 1;
              off_the_edge_facing = true;
            }
            else if (facing == 3) {
              y -= 1;
              off_the_edge_facing = true;
            }
          }
          // West edge facings not checked yet
          if (!off_the_edge_facing) {
            // West edge facing southwest
            if (facing == 5) {
              y -= 1;
              off_the_edge_facing = true;
            // West edge facing west
            } 
            else if (facing == 6) {
              if (ctx.GetRandom().GetInt(0, 2)) y += 1;
              else y -= 1;
              off_the_edge_facing = true;
            }
            // West edge facing northwest                                                   
            else if (facing == 7) {
              y += 1;
              off_the_edge_facing = true;
            }
          }
        }
        // East edge..
        else if (x == (x_size - 1)) {
          // Northeast corner
          if (y == 0) {
            if (facing == 0 || facing == 1 || facing == 2) {
              if (ctx.GetRandom().GetInt(0, 2)) x -= 1;
              else y += 1;
              off_the_edge_facing = true;
            }
            if (facing == 3) {
              y += 1;
              off_the_edge_facing = true;
            }
            if (facing == 7) {
              x -= 1;
              off_the_edge_facing = true;
            }
          }
          // Southeast corner
          else if (y == (y_size - 1)) {
            if (facing == 2 || facing == 3 || facing == 4) {
              if (ctx.GetRandom().GetInt(0, 2)) x -= 1;
              else y -= 1;
              off_the_edge_facing = true;
            }
            else if (facing == 1) {
              y -= 1;
              off_the_edge_facing = true;
            }
            else if (facing == 5) {
              x -= 1;
              off_the_edge_facing = true;
            }
          }
          // East edge facings not checked yet
          if (!off_the_edge_facing) {
            // East edge facing northeast
            if (facing == 1) {
              y -= 1;
              off_the_edge_facing = true;
            // East edge facing east
            } 
            else if (facing == 2) {
              if (ctx.GetRandom().GetInt(0, 2)) y += 1;
              else y -= 1;
              off_the_edge_facing = true;
            }
            // East edge facing southeast
            else if (facing == 3) {
              y += 1;
              off_the_edge_facing = true;
            }
          }
        }
        // North edge..
        else if (y == 0) {
          // North edge facing northwest
          if (facing == 7) {
            x -= 1;
            off_the_edge_facing = true;
          // North edge facing north
          } 
          else if (facing == 0) {
            if (ctx.GetRandom().GetInt(0, 2)) x += 1;
            else x -= 1;
            off_the_edge_facing = true;
          }
          // North edge facing northeast
          else if (facing == 1) {
            x += 1;
            off_the_edge_facing = true;
          }
        }
        // South edge..
        else if (y == (y_size - 1)) {
          // South edge facing southeast
          if (facing == 3) {
            x += 1;
            off_the_edge_facing = true;
          // South edge facing south
          } 
          else if (facing == 4) {
            if (ctx.GetRandom().GetInt(0, 2)) x += 1;
            else x -= 1;
            off_the_edge_facing = true;
          }
          // South edge facing southwest
          else if (facing == 5) {
            x -= 1;
            off_the_edge_facing = true;
          }
        }
      }
    }

    // Torus world geometry or not a bounded outward facing edge..
    if (!off_the_edge_facing || world_geometry == 2) {
      // North facing
      if ((facing == 0) || (facing == 1) || (facing == 7)) {
        y = (y - 1 + y_size) % y_size;
      }

      // South facing
      if ((facing == 3) || (facing == 4) || (facing == 5)) {
        y = (y + 1) % y_size;
      }

      // East facing
      if ((facing == 1) || (facing == 2) || (facing == 3)) {
        x = (x + 1) % x_size;
      }

      // West facing
      if ((facing == 5) || (facing == 6) || (facing == 7)) {
        x = (x - 1 + x_size) % x_size;
      }
    }

    const int new_cell_id = y * x_size + x;

    // Store the faced cell id
    m_avatars[av_num].av_faced_cell = new_cell_id;
  }
}

// Sets the avatar's cell's data
void cPopulationInterface::SetAVCellData(const int newData, const int org_id, int av_num)
{
  // If the avatar exists..
  if (av_num < GetNumAV()) {
    m_world->GetPopulation().GetCell(m_avatars[av_num].av_cell_id).SetCellData(newData, org_id);
  }
}

// Move input avatar into faced cell
bool cPopulationInterface::MoveAV(cAvidaContext& ctx, int av_num)
{
  // If the avatar exists..
  bool success = false;
  if (av_num < GetNumAV()) {
    // Move the avatar into the faced cell
    int src_id = m_avatars[av_num].av_cell_id;
    int dest_id = m_avatars[av_num].av_faced_cell;
    int true_cell = m_cell_id;
    if (m_world->GetPopulation().MoveOrganisms(ctx, src_id, dest_id, true_cell)) {
      return SetAVCellID(m_avatars[av_num].av_faced_cell, av_num);
    }
  }
  return success;
}

// Rotate the avatar by input increment, then set the new faced cell
bool cPopulationInterface::RotateAV(cAvidaContext& ctx, int increment, int av_num)
{
  // If the avatar exists..
  if (av_num < GetNumAV()) {
    if (increment >= 0) {
      increment %= 8;
    } else {
      increment = -increment;
      increment %= 8;
      increment = -increment;
    }
    // Adjust facing by increment
    int new_facing = (m_avatars[av_num].av_facing + increment + 8) % 8;
    SetAVFacing(ctx, new_facing);
    return true;
  }
  return false;
}

// Returns a random input or output avatar from the faced cell
cOrganism* cPopulationInterface::GetRandFacedAV(cAvidaContext& ctx, int av_num)
{
  // If the avatar exists..
  if (av_num < GetNumAV()) {
    return m_world->GetPopulation().GetCell(m_avatars[av_num].av_faced_cell).GetRandAV(ctx);
  }
  return NULL;
}

// Returns a random predator (input) avatar from the faced cell
cOrganism* cPopulationInterface::GetRandFacedPredAV(int av_num)
{
  // If the avatar exists..
  if (av_num < GetNumAV()) {
    return m_world->GetPopulation().GetCell(m_avatars[av_num].av_faced_cell).GetRandPredAV();
  }
  return NULL;
}

// Returns a random prey (output) avatar from the faced cell
cOrganism* cPopulationInterface::GetRandFacedPreyAV(int av_num)
{
  // If the avatar exists..
  if (av_num < GetNumAV()) {
    return m_world->GetPopulation().GetCell(m_avatars[av_num].av_faced_cell).GetRandPreyAV();
  }
  return NULL;
}

// Returns an array of all avatars in the organism's avatar's faced cell
Apto::Array<cOrganism*> cPopulationInterface::GetFacedAVs(int av_num)
{
  // If the avatar exists..
  if (av_num < GetNumAV()) {
    return m_world->GetPopulation().GetCell(m_avatars[av_num].av_faced_cell).GetCellAVs();
  }
  Apto::Array<cOrganism*> null_array(0);
  return null_array;
}

//Returns an array of all avatars in the organism's avatar's cell
Apto::Array<cOrganism*> cPopulationInterface::GetCellAVs(int cell_id, int av_num)
{
  //If the avatar exists...
  if (av_num < GetNumAV()) {
    return m_world->GetPopulation().GetCell(cell_id).GetCellAVs();
  }
  Apto::Array<cOrganism*> null_array(0);
  return null_array;
}

// Returns an array of all prey avatars in the organism's avatar's faced cell
Apto::Array<cOrganism*> cPopulationInterface::GetFacedPreyAVs(int av_num)
{
  // If the avatar exists..
  if (av_num < GetNumAV()) {
    return m_world->GetPopulation().GetCell(m_avatars[av_num].av_faced_cell).GetCellOutputAVs();
  }
  Apto::Array<cOrganism*> null_array(0);
  return null_array;
}

// Returns the avatar's cell resources
const Apto::Array<double>& cPopulationInterface::GetAVResources(cAvidaContext& ctx, int av_num)
{
  assert(av_num < GetNumAV());
  return m_world->GetPopulation().GetResources().GetCellResources(m_avatars[av_num].av_cell_id, ctx);
}

double cPopulationInterface::GetAVResourceVal(cAvidaContext& ctx, int res_id, int av_num)
{
  assert(av_num < GetNumAV());
  return m_world->GetPopulation().GetResources().GetCellResVal(ctx, m_avatars[av_num].av_cell_id, res_id);
}

// Returns the avatar's faced cell's resources
const Apto::Array<double>& cPopulationInterface::GetAVFacedResources(cAvidaContext& ctx, int av_num)
{
  assert(av_num < GetNumAV());
  return m_world->GetPopulation().GetResources().GetCellResources(m_avatars[av_num].av_faced_cell, ctx);
}

// Returns the avatar's faced cell's resources
double cPopulationInterface::GetAVFacedResourceVal(cAvidaContext& ctx, int res_id, int av_num)
{
  assert(av_num < GetNumAV());
  return m_world->GetPopulation().GetResources().GetCellResVal(ctx, m_avatars[av_num].av_faced_cell, res_id);
}

// Updates the avatar's cell resources
void cPopulationInterface::UpdateAVResources(cAvidaContext& ctx, const Apto::Array<double>& res_change, int av_num)
{
  // If the avatar exists..
  if (av_num < GetNumAV()) {
    m_world->GetPopulation().GetResources().UpdateCellResources(ctx, res_change, m_avatars[av_num].av_cell_id);
  }
}

