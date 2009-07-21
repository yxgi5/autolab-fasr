/***************************************************************************
 *   Copyright (C) 2009 by Jens
 *   jwawerla@sfu.ca
 *                                                                         *
 *   This program is free software; you can redistribute it and/or modify  *
 *   it under the terms of the GNU General Public License as published by  *
 *   the Free Software Foundation; either version 2 of the License, or     *
 *   (at your option) any later version.                                   *
 *                                                                         *
 *   This program is distributed in the hope that it will be useful,       *
 *   but WITHOUT ANY WARRANTY; without even the implied warranty of        *
 *   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the         *
 *   GNU General Public License for more details.                          *
 *                                                                         *
 *   You should have received a copy of the GNU General Public License     *
 *   along with this program; if not, write to the                         *
 *   Free Software Foundation, Inc.,                                       *
 *   59 Temple Place - Suite 330, Boston, MA  02111-1307, USA.             *
 ***************************************************************************
 * $Log: taskmanager.cpp,v $
 * Revision 1.11  2009-04-25 16:08:26  jwawerla
 * *** empty log message ***
 *
 * Revision 1.10  2009-04-11 05:27:53  vaughan
 * re-enabled forcing off odd-dimensioned map grids
 *
 * Revision 1.9  2009-04-11 00:39:21  vaughan
 * fixed rasterizer scaling bug
 *
 * Revision 1.8  2009-04-08 22:40:41  jwawerla
 * Hopefully ND interface issue solved
 *
 * Revision 1.7  2009-04-07 20:42:04  vaughan
 * still crashy
 *
 * Revision 1.6  2009-04-05 01:16:51  jwawerla
 * still debuging wavefront map
 *
 * Revision 1.5  2009-04-04 01:21:38  vaughan
 * working on large-scale demo
 *
 * Revision 1.4  2009-04-03 16:57:43  jwawerla
 * Some bug fixing
 *
 * Revision 1.3  2009-03-31 23:52:59  jwawerla
 * Moved cell index from float to int math
 *
 * Revision 1.2  2009-03-31 04:27:33  jwawerla
 * Some bug fixing
 *
 * Revision 1.1  2009-03-31 01:42:01  jwawerla
 * Task definitions moved to task manager and stage world file
 *
 * Revision 1.1.1.1  2009-03-15 03:52:02  jwawerla
 * First commit
 *
 *
 ***************************************************************************/
#include "taskmanager.h"
#include "error.h"
#include "utilities.h"

//-----------------------------------------------------------------------------
CTaskManager::CTaskManager ( Stg::Model* mod )
{
  printf("CTaskManager::CTaskManager \n");
  char* mapModelName = NULL;
  float wavefrontCellSize;
  mSimTime = 0;
  mNextModificationTime = 0.0;
  mScriptName = NULL;
  mTaskList.clear();
  mRobotList.clear();

  if ( mod == NULL )
  {
    PRT_ERR0 ( "No Stage model given " );
    mStgModel = NULL;
  }
  else
  {
    mStgModel = mod;
    mStgModel->AddUpdateCallback ( ( Stg::stg_model_callback_t ) stgUpdate, this );
    if ( ! mStgModel->GetPropertyStr ( "scriptname", &mScriptName,  NULL ) )
      PRT_WARN0 ( "No script specified " );
    else
    {
      PRT_MSG1 ( 5, "Loading script %s", mScriptName );
    }

    if ( ! mod->GetPropertyFloat ( "wavefrontcellsize", &wavefrontCellSize,  0.1 ) )
      PRT_WARN1 ( "CTaskManager %s has no parameter wavefrontcellsize specified in worldfile.",
                  mod->Token() );

    if ( ! mod->GetPropertyStr ( "mapmodel", &mapModelName,  NULL ) )
    {
      PRT_ERR1 ( "robot %s has no mapmodel string specified in worldfile.",
                 mod->Token() );
      exit ( -1 );
    }

    Stg::Model* mapModel = mod->GetWorld()->GetModel ( mapModelName );
    if ( ! mapModel )
    {
      PRT_ERR1 ( "worldfile has no model named \"%s\"", mapModelName );
      exit ( -1 );
    }

    // get an occupancy grid from the Stage model
    Stg::Geom geom = mapModel->GetGeom();
    int width = geom.size.x / wavefrontCellSize;
    int height = geom.size.y / wavefrontCellSize;

    if ( ( width % 2 ) == 0 )
      width++;

    if ( ( height % 2 ) == 0 )
      height++;

    uint8_t* cells = new uint8_t[ width * height ];
    mapModel->Rasterize ( cells,
                          width, height,
                          wavefrontCellSize, wavefrontCellSize );
    mWaveFrontMap = new CWaveFrontMap ( new CGridMap ( cells,
                                        width, height,
                                        wavefrontCellSize ),
                                        "map" );
    if ( cells )
      delete[] cells;
  }
  // enforce first script parsing
  loadScript();
}
//-----------------------------------------------------------------------------
CTaskManager::~CTaskManager()
{
  CWorkTask* task = NULL;
  std::list<CWorkTask*>::iterator it;

  for ( it = mTaskList.begin(); it != mTaskList.end(); it++ )
  {
    task = *it;
    if ( task )
      delete task;
    it = mTaskList.erase ( it );
  }
}
//-----------------------------------------------------------------------------
CTaskManager* CTaskManager::getInstance ( Stg::Model* mod )
{
  /** The only instance of this class */
  static CTaskManager* instance = NULL;

  if ( instance == NULL )
    instance = new CTaskManager ( mod );

  return instance;
}
//-----------------------------------------------------------------------------
void CTaskManager::registerRobot ( ABaseRobotCtrl* robot )
{
  std::list<CWorkTask*>::iterator it;

  mRobotList.push_back ( robot );

  for ( it = mTaskList.begin(); it != mTaskList.end(); it++ )
  {
    robot->addWorkTask ( *it );
  }
}
//-----------------------------------------------------------------------------
int CTaskManager::stgUpdate ( Stg::Model* mod, CTaskManager* taskManager )
{
  taskManager->mSimTime = mod->GetWorld()->SimTimeNow() / 1e6;

  if ( taskManager->mSimTime == taskManager->mNextModificationTime )
    taskManager->loadScript();

  return 0; // ok
}
//-----------------------------------------------------------------------------
int CTaskManager::loadScript()
{
  CWorkTask* newWorkTask = NULL;
  bool fgFound;
  std::list<CWorkTask*>::iterator it;
  float timestamp = 0.0;
  int capacity;
  float productionRate = 0.0;
  float reward = 0.0;
  char* taskName = new char[20];
  char* sourceName = new char[20];
  char* sinkName = new char[20];
  FILE* fp = NULL;

  fp = fopen ( mScriptName, "r" );
  if ( fp == NULL )
  {
    PRT_ERR1 ( "Failed to open script file %s", mScriptName );
    return 0; // error
  }

  mNextModificationTime = INFINITY;

  while ( feof ( fp ) == false )
  {
    if ( fscanf ( fp, "TASK: %f %s %s %s %d %f %f\n", &timestamp, taskName,
                  sourceName, sinkName, &capacity, &productionRate,
                  &reward ) != 7 )
    {
      PRT_ERR1 ( "While reading script file %s", mScriptName );
    }

    // check if this task is already in the list of tasks, if not add it to the list
    fgFound = false;
    for ( it = mTaskList.begin(); it != mTaskList.end(); it++ )
    {
      if ( strcmp ( ( *it )->getName(), taskName ) == 0 )
      {
        fgFound = true;
        newWorkTask = *it;
      }
    }
    if ( fgFound == false )
    {
      newWorkTask = new CWorkTask ( mStgModel->GetWorld()->GetModel ( sourceName ),
                                    mStgModel->GetWorld()->GetModel ( sinkName ),
                                    taskName );
      mTaskList.push_back ( newWorkTask );

      mWaveFrontMap->calculateWaveFront ( newWorkTask->mSource->getLocation(),
                                          CWaveFrontMap::USE_MAP_ONLY );
      newWorkTask->setSrcSinkDistance ( mWaveFrontMap->calculatePlanFrom (
                                            newWorkTask->mSink->getLocation() ) );
    }

    if ( ( timestamp == mSimTime ) && ( newWorkTask != NULL ) )
    {
      newWorkTask->setStorageCapacity ( capacity );
      newWorkTask->setReward ( reward );
      newWorkTask->setProductionRate ( productionRate );
    }

    if ( ( timestamp > mSimTime ) && ( timestamp < mNextModificationTime ) )
      mNextModificationTime = timestamp;

  } // while

  fclose ( fp );

  return 1; // success
}
//-----------------------------------------------------------------------------


