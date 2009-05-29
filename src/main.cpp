
#include "stage.hh"
#include "RapiStage"
#include "baserobotctrl.h"
#include "staticpolicyrobotctrl.h"
#include "looppolicyrobotctrl.h"
#include "taskmanager.h"
#include "RapiGui"

extern "C" int Init ( Stg::Model* mod )
{
  Rapi::CLooseStageRobot* robot;
  Rapi::CGui* gui = Rapi::CGui::getInstance(0, NULL);
  ABaseRobotCtrl* robotCtrl;
  CTaskManager* taskManager;
  CBroadCast* broadCast;
  static CCharger charger ( mod->GetWorld()->GetModel ( "charger" ) );
  static CDestination depotDestination ( mod->GetWorld()->GetModel ( "depot" ) );

  broadCast = CBroadCast::getInstance ( 10.0 );
  taskManager = CTaskManager::getInstance (
                  mod->GetWorld()->GetModel ( "taskmanager" ) );

  // init general stuff
  ErrorInit ( 4, false );
  initRandomNumberGenerator();

  // create robot and its controller
  robot = new Rapi::CLooseStageRobot ( mod );
  //robotCtrl =  new CStaticPolicyRobotCtrl ( robot );
  robotCtrl = new CLoopPolicyRobotCtrl ( robot );

  gui->registerRobot(robot);
  // inform robot controller about tasks, charger and depot
  taskManager->registerRobot ( robotCtrl );
  robotCtrl->addCharger ( &charger );
  robotCtrl->setDepot ( &depotDestination );

  return 0; // ok
}