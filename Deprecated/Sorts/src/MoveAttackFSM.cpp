/*
    This file is part of Sorts, an interface between Soar and ORTS.
    (c) 2006 James Irizarry, Sam Wintermute, and Joseph Xu

    Sorts is free software; you can redistribute it and/or modify
    it under the terms of the GNU General Public License as published by
    the Free Software Foundation; either version 2 of the License, or
    (at your option) any later version.

    Sorts is distributed in the hope that it will be useful,
    but WITHOUT ANY WARRANTY; without even the implied warranty of
    MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
    GNU General Public License for more details.

    You should have received a copy of the GNU General Public License
    along with Sorts; if not, write to the Free Software
    Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA  02110-1301  USA    
*/
#include"MoveAttackFSM.h"
#include<iostream>
#include<cmath>

#include "Sorts.h"
using namespace std;

#define CLASS_TOKEN "MOVEFSM"
#define DEBUG_OUTPUT false 
#include "OutputDefinitions.h"

#define TOLERANCE 9 // for waypoints, squared
#define VEERWIDTH 6  // radius of worker == 3

// these quantities are determined randomly within a range
// veercount: number of veers allowed in a row
// if exceeded, veering does nothing (keep pushing ahead)
#define MIN_VEERCOUNT 1 
#define MAX_VEERCOUNT_DIFF 4 //  min < value < min+maxdiff

// counter: number of consecutive in-place veers before FSM_FAILURE returned
// i.e. the max number of cycles to allow zero speed before giving up
#define MIN_COUNTER 2
#define MAX_COUNTER_DIFF 3 // min < value <  min+maxdiff

// insert fake workers in the pathfinder at the middle waypoint-
// this is essentially a collaborative pathfinding hack, it decreases the
// number of collisions if multiple units are moving through the same region
#define WAYPOINT_IMAGINARY_WORKERS


// do a collision-check in front of the current location, and veer if needed
#define RADAR
  
/*
  radar parameters used by the veerAhead function
  look in front of the gob, if the spot RADAR_FORWARD_DIST ahead
  is blocked, veer around it (if that spot is not blocked)

    radar forward distance
        v 
   obj --- obstacle
      \radar veer angle
       \
        \radar angle dist (length of line)
         \
          go here instead

   if the distance between the obstacle and the new spot is x,
   rva = tan-1(x/rfd) (in radians!)
   rad = sqrt(x^2 + rfd^2)
*/


#define RADAR_FORWARD_DIST_SQ RADAR_FORWARD_DIST * RADAR_FORWARD_DIST

// x=6, rfd 7
#define RADAR_FORWARD_DIST 7 // must be > 6, or the worker will collide 
#define RADAR_VEER_ANGLE .70862
#define RADAR_ANGLE_DIST 9.22

//x=10, rfd 7
//#define RADAR_FORWARD_DIST 7 // must be > 6, or the worker will collide 
//#define RADAR_VEER_ANGLE .96
//#define RADAR_ANGLE_DIST 12.2


MoveAttackFSM::MoveAttackFSM(GameObj* go) 
            : FSM(go) {
  name = OA_MOVE_INTERNAL;

  vec_count = 0;
  veerCount = 0;
  
  currentLocation.x = (*gob->sod.x);
  currentLocation.y = (*gob->sod.y);
  precision = 400;
  lastRight = false;
  lastLocation.x = -1;
  lastLocation.y = -1;
  nextWPIndex = -1;
  usingIWWP = false;
  idleTime = 0;
  direction = 1;
  influence = Sorts::influence;
}

MoveAttackFSM::~MoveAttackFSM() {
}

int MoveAttackFSM::update() {
  currentLocation.x = (*gob->sod.x);
  currentLocation.y = (*gob->sod.y);
  msg << "current location: " << currentLocation.x << "," 
                              << currentLocation.y << endl;

  if (gob->is_pending_action()) {
    msg << "action has not taken affect!\n";
    return FSM_RUNNING;
  }

  // veer in a random direction (take this out for alternate veering)
  if (rand()% 2) {
    lastRight = true;
  }
  else {
    lastRight = false;
  }
  
 switch(state) {

	case IDLE:
    msg << "idle\n";
    gob->set_action("move", moveParams);
	  state = MOVING;
    counter = 0;
    counter_max = MIN_COUNTER + (rand()%MAX_COUNTER_DIFF );
   break;
  
  case ALREADY_THERE:
    msg << "already there\n";
    return FSM_SUCCESS; 
    break;

  case UNREACHABLE:
    return FSM_UNREACHABLE;
    break;

  case STUCK:
    return FSM_STUCK;
    break;

	case MOVING:
	  msg << "moving @ speed " << *gob->sod.speed << "\n";
    const ServerObjData &sod = gob->sod;
    double distToTarget = squaredDistance(*sod.x, *sod.y, target.x, target.y);

	  // if speed drops to 0
    // and we are not there, failure
    if (*sod.speed == 0 or (currentLocation == lastLocation)) {
      if (*sod.speed > 0) {
        msg << "moving, but stuck, somehow.\n";
      }
      if ((nextWPIndex >= 0 and distToTarget < TOLERANCE)
         or (nextWPIndex == -1 and distToTarget <= precision)) {
        counter = 0;
        counter_max = MIN_COUNTER + (rand()%MAX_COUNTER_DIFF );
        dbg << "using counter of " << counter_max << endl;
        // If you arrived, then check if 
        // there is another path segment to traverse
        if (nextWPIndex >= 0) {
          if (nextWPIndex+1 < path.locs.size() and
              target == path.locs[nextWPIndex+1]) {
            // this means we reached a "real" waypoint (not one set by veering)
            veerCount = 0;
          }
          if (usingIWWP and target == imaginaryWorkerWaypoint) {
            Sorts::terrainModule->
              removeImaginaryWorker(imaginaryWorkerWaypoint);
            usingIWWP = false;
          }
          target.x = moveParams[0] = path.locs[nextWPIndex].x;
          target.y = moveParams[1] = path.locs[nextWPIndex].y;
          nextWPIndex--;
	        gob->set_action("move", moveParams);
        }
        else { 
          // nextWPIndex == -1: we must be there
          dbg << "dist: " << distToTarget << endl;
          dbg << "target: " << target.x << target.y << endl;
          // Otherwise, you are at your goal
          if (usingIWWP and target == imaginaryWorkerWaypoint) {
            Sorts::terrainModule->
              removeImaginaryWorker(imaginaryWorkerWaypoint);
            usingIWWP = false;
          }
          return FSM_SUCCESS;     
        }
      }
      else {//Try again
        if (counter++ < counter_max) {
          if (Sorts::OrtsIO->getSkippedActions() == 0) {
            veerRight();
            // veerRight may or may not adjust target
          }
          moveParams[0] = target.x;
          moveParams[1] = target.y;
	        gob->set_action("move", moveParams);
        }
        else {
          msg << "failed, must repath\n";
          clearWPWorkers();
          return FSM_FAILURE;
        }
      }
    }
    else if (distToTarget <= TOLERANCE and nextWPIndex >= 0) {
      counter = 0;
      dbg << "in-motion dir change\n";
      if (nextWPIndex+1 < path.locs.size() and
          target == path.locs[nextWPIndex+1]) {
        // this means we reached a "real" waypoint (not one set by veering)
        veerCount = 0;
        if (usingIWWP and target == imaginaryWorkerWaypoint) {
          Sorts::terrainModule->
            removeImaginaryWorker(imaginaryWorkerWaypoint);
          usingIWWP = false;
        }
      }
        
      target.x = moveParams[0] = path.locs[nextWPIndex].x;
      target.y = moveParams[1] = path.locs[nextWPIndex].y;
      nextWPIndex--;
      gob->set_action("move", moveParams);
    }
    else {
      // moving, not at waypoint
      counter = 0;
#ifdef RADAR
      if (veerAhead((int)distToTarget)) {
        // may or may not change target
        moveParams[0] = target.x;
        moveParams[1] = target.y;
        gob->set_action("move", moveParams);
        msg << "pre-veering\n";
      }
#endif

#ifdef MAGNETISM    
      cout << "MOVEFSM: Magnetized\n";
      if(getMoveVector()) {
        cout<<"MOVEFSM: MoveParams: "<<moveParams[0]<<","<<moveParams[1]<<"\n";
        gob->set_action("move",moveParams);
      }
#endif
    }

    break;

    
	} // switch (state)
  lastLocation = currentLocation;
 
  return FSM_RUNNING;
}

void MoveAttackFSM::init(vector<sint4> p) 
{
  FSM::init(p);
  
  SortsSimpleTerrain::Loc l;
  l.x = p[0];
  l.y = p[1];
  int pathLength;
  bool forcePathfind = false;
 
  if (p.size() >= 3 && p[2] != -1) {
    // third parameter specifies how close to the target we must get
    precision = p[2];
    precision *= precision; // since we use distance squared
  }

  if (p.size() == 4) {
    // fourth param presence means force a pathfind
    // this is used when we know the destination should be reachable, but may
    // be close to unreachable locations, so we shouldn't skip the pathfind if 
    // we have an imag. obs. collision

    forcePathfind = true;
  }
  
  msg << "initialized. Path " << *gob->sod.x << "," << *gob->sod.y << "->"
      << l.x << "," << l.y << endl;

  clearWPWorkers();
  Circle targLoc(l.x, l.y, *gob->sod.radius);
  bool terrainCollide = Sorts::spatialDB->hasTerrainCollision(targLoc);
  if (not forcePathfind and terrainCollide) {
    msg << "Terrain collision, not pathfinding.\n";
    state = UNREACHABLE;
  //  Sorts::canvas.makeTempCircle(l.x, l.y, *gob->sod.radius, 9999)->setShapeColor(255, 255, 255);
  }
  else {
    Sorts::terrainModule->findPath(gob, l, path);
    pathLength = path.locs.size();
    
    for (int i=0; i<pathLength; i++) {
      dbg << "loc " << i << " " 
          << path.locs[i].x << ", "<< path.locs[i].y << endl;
    }
    
    nextWPIndex = path.locs.size()-1;
    moveParams.clear();
    if (path.locs.size() > 0) {
      if (path.locs.size() > 1
          and collision(path.locs[nextWPIndex].x, path.locs[nextWPIndex].y)) {
        nextWPIndex--;
        msg << "first waypoint is inside an object, avoiding.\n";
        if (path.locs.size() > 2
          and collision(path.locs[nextWPIndex].x, path.locs[nextWPIndex].y)) {
          nextWPIndex--;
          msg << "second waypoint is also inside an object, avoiding.\n";
        }
          
      }

      if ((nextWPIndex-1) > 0) {
        usingIWWP = true;
        imaginaryWorkerWaypoint = path.locs[((nextWPIndex-1)/2)];
        Sorts::terrainModule->insertImaginaryWorker(imaginaryWorkerWaypoint);
      }
        
      moveParams.push_back(path.locs[nextWPIndex].x);
      moveParams.push_back(path.locs[nextWPIndex].y);
      nextWPIndex--;
      state = IDLE;
      target.x = moveParams[0];
      target.y = moveParams[1];
    }
    else if (squaredDistance(*gob->sod.x, *gob->sod.y, l.x, l.y) 
          <= precision) {
      target.x = l.x;
      target.y = l.y;
      state = ALREADY_THERE; 
    }
    else {
     // msg << "STUCK" << endl;
     // state = STUCK;
     // replaced with the above
      coordinate c(l.x,l.y);
      if (not isReachableFromBuilding(l)) { 
        msg << "adding unreachable location.\n";
        Sorts::spatialDB->addImaginaryObstacle(c);
        state = UNREACHABLE;
      }
      else {
        msg << "pathfind fails from my point only!\n";
        state = STUCK;
      }
      
    }
  }
}

void MoveAttackFSM::initNoPath(vector<sint4> p) 
{
  FSM::init(p);
 
  SortsSimpleTerrain::Loc l;
  l.x = p[0];
  l.y = p[1];

  clearWPWorkers();
  if (p.size() == 3) {
    // third parameter specifies how close to the target we must get
    precision = p[2];
    precision *= precision; // since we use distance squared
  }

  path.locs.clear();
  path.locs.push_back(l);
 
  usingIWWP = false;
  nextWPIndex = path.locs.size()-1;
  moveParams.clear();
  moveParams.push_back(path.locs[nextWPIndex].x);
  moveParams.push_back(path.locs[nextWPIndex].y);
  nextWPIndex--;
  state = IDLE;
  target.x = moveParams[0];
  target.y = moveParams[1];
}

void MoveAttackFSM::stop() {
  Vector<sint4> params;
  params.push_back(*gob->sod.x);
  params.push_back(*gob->sod.y);
  params.push_back(0);
  gob->set_action("move", params);
  state = ALREADY_THERE;
  clearWPWorkers();
}

void MoveAttackFSM::clearWPWorkers() {
  if (usingIWWP) {
    Sorts::terrainModule->removeImaginaryWorker(imaginaryWorkerWaypoint);
    usingIWWP = false;
  }
}

void MoveAttackFSM::veerRight() {
  // if there is open space to the right,
  // make that the new target.
  // if that happens, increment nextWPIndex so the current waypoint
  // is moved to after we get to the right
  // do not allow multiple rightward moves! better to just return failure
  // so the FSM is reinitted with a new path

  if (veerCount > (rand()%MAX_VEERCOUNT_DIFF + MIN_VEERCOUNT)) {
    msg << "already veered too much!\n";
    return;
  }
 
  double deltaX = target.x - *gob->sod.x;
  double deltaY = target.y - *gob->sod.y;
 
  double headingAngle = atan2(deltaY, deltaX); // -pi to pi (deltaX == 0 ok)
  if (lastRight) {
    headingAngle -= (PI/2.0);
  }
  else {
    headingAngle += (PI/2.0);
  }
 
  int width = VEERWIDTH + rand()%8;
  int newX = *gob->sod.x + (int)(width*cos(headingAngle));
  int newY = *gob->sod.y + (int)(width*sin(headingAngle));

  if (!collision(newX, newY)) {
    dbg << "trying right.\n";
    if ((nextWPIndex+1 < path.locs.size()) and
        target == path.locs[nextWPIndex+1]) {
      nextWPIndex++;
    }
    msg << "veer " << *gob->sod.x << "," << *gob->sod.y << "->"
        << newX << "," << newY << " on the way to " 
        << target.x << "," << target.y << endl;
    target.x = newX;
    target.y = newY;
    veerCount++;
  }
  else {
    if (lastRight) {
      headingAngle += PI;
    }
    else {
      headingAngle -= PI;  
    }
    newX = *gob->sod.x + (int)(width*cos(headingAngle));
    newY = *gob->sod.y + (int)(width*sin(headingAngle));

    msg << "veering left (obstacles be damned).\n";
    if ((nextWPIndex+1 < path.locs.size()) and
        target == path.locs[nextWPIndex+1]) {
      nextWPIndex++;
    }
    msg << "veer " << *gob->sod.x << "," << *gob->sod.y << "->"
        << newX << "," << newY << " on the way to " 
        << target.x << "," << target.y << endl;
    target.x = newX;
    target.y = newY;
    veerCount++;
  }

  lastRight = not lastRight;
  return;
}

bool MoveAttackFSM::veerAhead(int distToTargetSq) {
  // estimate where we will be next cycle
  // if there is something there, try the space to the right
  // make that the new target if it is clear
  // if that happens, increment nextWPIndex so the current waypoint
  // is moved to after we get to the right
  // do not allow multiple rightward moves! better to just return failure
  // so the FSM is reinitted with a new path

  if (veerCount > (rand()%MAX_VEERCOUNT_DIFF + MIN_VEERCOUNT)) {
    msg << "already veered too much!\n";
    return false;
  }
  if (distToTargetSq <= RADAR_FORWARD_DIST_SQ) {
    msg << "not veering, target is too close\n";
    return false;
  }
 
  double deltaX = target.x - *gob->sod.x;
  double deltaY = target.y - *gob->sod.y;
 
  double headingAngle = atan2(deltaY, deltaX); // -pi to pi (deltaX == 0 ok)

  int newX = *gob->sod.x + (int)(RADAR_FORWARD_DIST*cos(headingAngle));
  int newY = *gob->sod.y + (int)(RADAR_FORWARD_DIST*sin(headingAngle));

  if (dynamicCollision(newX, newY)) {
    msg << "veerAhead sees an obstacle!\n";
    if (lastRight) {
      headingAngle -= RADAR_VEER_ANGLE;
    }
    else {
      headingAngle += RADAR_VEER_ANGLE;
    }
    newX = *gob->sod.x + (int)(RADAR_ANGLE_DIST*cos(headingAngle));
    newY = *gob->sod.y + (int)(RADAR_ANGLE_DIST*sin(headingAngle));
    if (!collision(newX, newY)) {
      if ((nextWPIndex+1 < path.locs.size()) and 
           target == path.locs[nextWPIndex+1]) {
        nextWPIndex++;
      }
      target.x = newX;
      target.y = newY;
      msg << "veering ahead!\n";
      veerCount++;
      lastRight = not lastRight;
      return true;
    }
    else {
      msg << "new path is blocked, not veering.\n";
      return false;
    }
  }

  dbg << "clear ahead, no veer\n";
  return false;
}

bool MoveAttackFSM::collision(int x, int y) {
  coordinate c(x,y);
  
  return Sorts::spatialDB->hasObjectCollision(c, *gob->sod.radius, gob);
}

bool MoveAttackFSM::dynamicCollision(int x, int y) {
  // return true if loc collides with a sheep or worker
  list<GameObj*> collisions;
  
  Sorts::spatialDB->getObjectCollisions(x, y, 6, NULL, collisions);
  dbg << x << "," << y << " collides with " << collisions.size() 
       << " things.\n";
  
  for (list<GameObj*>::iterator it = collisions.begin();
       it != collisions.end();
       it++) {
    if ((*it) == gob) {
      dbg << "gob collides w/self, ignoring\n";
    }
    else if ((*it)->bp_name() == "worker" or
             (*it)->bp_name() == "sheep") {
      msg << "collides with " << (*it)->bp_name() << endl;
      dbg << "loc: " << *(*it)->sod.x << "," << *(*it)->sod.y << endl;
      dbg << "radius " << *(*it)->sod.radius << endl; 
      return true;
    }
  }

  return false;
}
// MAGNETISM CODE

// Returns the actual move vector of the object
// New move coordinates are placed into the moveParams structure
bool MoveAttackFSM::getMoveVector()
{
 bool answer = false;
 /*// - Things don't change rapidly
 // - Return false is not enough time has passed
 //   - Need to figure out how much time is enough
 cout<<"MOVEFSM: Vec_count: "<<vec_count<<"\n";
// if(vec_count < 5)
 {
  vec_count++;
//  return answer;
 }
 answer = true;
 vec_count = 0;

 std::list<GameObj*> *neighbors = new list<GameObj*>;// = Sorts::spatialDB->getObjectsInRegion(*(gob->sod.x), *(gob->sod.y));
 std::list<GameObj*>::iterator it;
 
 float x = 0;
 float y = 0;

 // Add up the repulsion vectors
 for(it = neighbors->begin(); it!=neighbors->end(); it++)
 {
   // normalize? /dist^2
  x += (*(*it)->sod.x)-loc.x;
  y += (*(*it)->sod.y)-loc.y;
 }
 // Normalize the vector
 float d = sqrt((float)((loc.x-x)*(loc.x-x)+(loc.y-y)*(loc.y-y)));
 x /= d;
 y /= d;

 // Add in the attraction vector
 float x1 = target.x - loc.x;
 float y1 = target.y - loc.y;
 d = sqrt((float)((loc.x-x1)*(loc.x-x1)+(loc.y-y1)*(loc.y-y1)));

 cout<<"MOVEFSM: Size: "<< neighbors->size()<<"\n";
 cout<<"MOVEFSM: Repulsion Vector: ("<<x<<","<<y<<")\n";
 cout<<"MOVEFSM: Attraction Vector: ("<<x1/d<<","<<y1/d<<")\n";

 x += x1/d;
 y += y1/d;
 cout<<"Combined Vector: ("<<x<<","<<y<<")\n";
 
 SortsSimpleTerrain::Loc loc = getHeadingVector(static_cast<sint4>(x),static_cast<sint4>(y));
 
 moveParams[0] = loc.x;
 moveParams[1] = loc.y;
 */
 return answer;
}


SortsSimpleTerrain::Loc MoveAttackFSM::getHeadingVector(sint4 target_x, sint4 target_y)
{
 sint4 x = *(gob->sod.x);
 sint4 y = *(gob->sod.y);
 
 double m = (target_y-y)/(target_x-x);
 double b = (y-m*x);
 int quadrant;
 

 if(x<target_x)
  if(y<target_y)
   quadrant = 4;
  else
   quadrant = 1;
 else
  if(y<target_y)
   quadrant = 2; 
  else
   quadrant = 3;

 
 SortsSimpleTerrain::Loc loc;
 switch(quadrant){
    case 1:
     if((loc.x = static_cast<sint4>(-1*b/m)) > Sorts::OrtsIO->getMapXDim())
     {
      loc.x = Sorts::OrtsIO->getMapXDim();
      loc.y = static_cast<sint4>(m*loc.x+b);
     }
     else
      loc.y = 0;
     break;
    case 2:
     if((loc.x = static_cast<sint4>(-1*b/m)) < 0)
     {
      loc.x = 0;
      loc.y = static_cast<sint4>(b);
     }
     else
      loc.y = 0;
     break;
    case 3:
     if((loc.x = static_cast<sint4>((Sorts::OrtsIO->getMapYDim()-b)/m)) < 0)
     {
      loc.x = 0;
      loc.y = static_cast<sint4>(b);
     }
     else
      loc.y = Sorts::OrtsIO->getMapYDim();
     break;
    case 4:
     if((loc.x = static_cast<sint4>((Sorts::OrtsIO->getMapYDim()-b)/m)) > Sorts::OrtsIO->getMapXDim())
     {
      loc.x = Sorts::OrtsIO->getMapXDim();
      loc.y = static_cast<sint4>(m*loc.x+b);
     }
     else
      loc.y = Sorts::OrtsIO->getMapYDim();
     break;
    }
    
 cout<<"MOVEFSM: Heading Vector: ("<<loc.x<<","<<loc.y<<","<<quadrant<<")\n";
 return loc;
}

bool MoveAttackFSM::isReachableFromBuilding(SortsSimpleTerrain::Loc l) {
  // HACK: this always retuns false, unless it is inside the CC (which kills
  // mining)
  SortsSimpleTerrain::Path tempPath;
  GameObj* sourceObj = Sorts::OrtsIO->getReachabilityObject();

  if (sourceObj == NULL) return false;

  // sourceObj (usually the controlCenter) is always reachable,
  // but has path length 0 to itself
  if (gobX(sourceObj) == l.x && gobY(sourceObj) == l.y) {
    return true;
  }
  
  Sorts::terrainModule->findPath(sourceObj, 
                                 l, tempPath);
  return (tempPath.locs.size() > 0);
}

void MoveAttackFSM::panic() {
  vector<sint4> tempVec;
  tempVec.push_back(*gob->sod.x + ((int)rand() % 30) -15);
  tempVec.push_back(*gob->sod.y + ((int)rand() % 30) -15);
  initNoPath(tempVec);
}


/****************************************************************/
/*************************** Interns ****************************/
/****************************************************************/

void MoveAttackFSM::traverse(sint4 locx, sint4 locy, ScalarPoint goal){
  std::cout << "Traverse" << endl;
  ScalarPoint moveTo;
  sint4 dx = goal.x-locx;
  sint4 dy = goal.y-locy;
  
  if(dy == 0){
    moveTo.x = locx;
    moveTo.y = locy + (20 * direction);
    gob->set_action("move", move(moveTo, *gob->sod.max_speed));
    return;
  }

  /* Here, we calculate the slope, then normalize our
   * movement vector, multiply the resulting components by
   * the number of steps we want to move, and add this to
   * our original location vector to determine our expected
   * position
   */
  double slope = -((double) dx) / ((double) dy);  
  double mag = sqrt(1+(slope*slope));

  moveTo.x = locx + (int) (20.0 / mag * direction);
  moveTo.y = locy + (int) (20.0 / mag * slope * direction);

  gob->set_action("move", move(moveTo, *gob->sod.max_speed));
}


bool MoveAttackFSM::moveSimple(sint4 x, sint4 y){
  assert(x >= 0);
  assert(y >= 0);
  std::cout << gob << std::endl;
  // Check that the unit is moving
  ScalarPoint goal;
  goal.x = x;
  goal.y = y;  
  
  // If the unit has stopped
  if(*gob->sod.speed == 0){
    idleTime++;
      
    // Traverse in one direction
    if(idleTime % 2 == 0){
      direction *= -1;
      traverse(*gob->sod.x, *gob->sod.y, goal);
    }
    // Traverse in the other direction
    else if(idleTime % 2 == 1){
      traverse(*gob->sod.x, *gob->sod.y, goal);
    }
  }
  // Our unit is moving
  else{
    idleTime = 0;
    gob->set_action("move", move(goal, *gob->sod.max_speed));
  }
  return true;
}

/********************************************************************
 * MoveAttackFSM::move
 ********************************************************************/
Vector<sint4> MoveAttackFSM::move(ScalarPoint loc, sint4 speed){
  Vector<sint4> params;
  params.push_back(loc.x);
  params.push_back(loc.y);
  params.push_back(speed);
  return params;
}

/********************************************************************
 * MoveManager::moveAllInfluence
 ********************************************************************/
void MoveAttackFSM::moveForces(sint4 x, sint4 y){
  assert(x >= 0);
  assert(y >= 0);
  assert(influence != NULL);

  // Check that all units are moving
  ScalarPoint goal;
  goal.x = x;
  goal.y = y;  

  // If our unit has stopped
  if(*gob->sod.speed == 0){
    idleTime++;
      
    // Traverse in one direction
    if(idleTime % 2 == 0){
      direction *= -1;
      traverse(*gob->sod.x, *gob->sod.y, goal);
    }
    // Traverse in the other direction
    else //(squad->idleTime[i] % 2 == 1)
      traverse(*gob->sod.x, *gob->sod.y, goal);
  }
  // Otherwise, our unit is moving
  else{
    idleTime = 0;
    gob->set_action("move", move(force_vector(goal), *gob->sod.max_speed));
  }
}

/********************************************************************
 * MoveManager::force_vector
 ********************************************************************/
ScalarPoint MoveAttackFSM::force_vector(ScalarPoint goal){
  cout << "CALCULATING FORCES" << endl;
  cout << "Test influence map: " << Sorts::influence->get_width_pixels() << endl;
  pair<sint4, sint4> friendlyF;
  pair<double, double> targetF, rockF;
  sint4 xpos = *gob->sod.x;
  sint4 ypos = *gob->sod.y;
  rockF.first = rockF.second = 0.0;
  
  // set weighting values
  const sint4 targetWeight = 80;
  const sint4 rockWeight = 10;
  
  // calculate direction to target
  sint4 dx = goal.x - xpos;
  sint4 dy = goal.y - ypos;
  double length  = sqrt(dx * dx + dy * dy);
  targetF.first  = targetWeight * (dx / length);
  targetF.second = targetWeight * (dy / length);
  
  // calculate effect of terrain
  for(dx= -50; dx < 50; dx += 16){
    for(dy = -50; dy < 50; dy += 16){
      if((xpos + dx) > 0 && (ypos + dy) > 0 &&
         (xpos + dx) < Sorts::influence->get_width_pixels() &&
	 (ypos + dy) < Sorts::influence->get_height_pixels()){
	double influenceValue = Sorts::influence->value_at(xpos+dx, ypos+dy);
	double length = sqrt(dx*dx + dy*dy);
   
	if(influenceValue > 1000.0){
          rockF.first  += -rockWeight* dx/length;
	  rockF.second += -rockWeight* dy/length;
        }
      }
    }
  }
  
  // calculate direction and speed of movement
  dx = (sint4) ((targetF.first  + rockF.first ));
  dy = (sint4) ((targetF.second + rockF.second));
  
  goal.x = xpos + dx;
  goal.y = ypos + dy;
    
  return goal;
}
