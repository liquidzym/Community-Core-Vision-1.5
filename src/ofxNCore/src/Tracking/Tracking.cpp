/*
*  Tracking.cpp
*
*  Created by Ramsin Khoshabeh on 5/4/08.
*  Copyright 2008 risenparadigm. All rights reserved.
*
*  Changelog:
*  08/15/08 -- Fixed a major bug in the track algorithm
*
*
*/

#include "Tracking.h"

BlobTracker::BlobTracker()
{
	IDCounter = 200;
	isCalibrating = false;
	camWidth = 320;
	camHeight = 240;
	//Initialise Object Blobs
	for(int i = 180;i<200;i++)
	{
		trackedObjects[i].id = -1;
		trackedObjects[i].lastTimeTimeWasChecked = 0;
	}
}

void BlobTracker::setCameraSize(int width,int height)
{
	camWidth = width;
	camHeight = height;
}

void BlobTracker::passInFiducialInfo(ofxFiducialTracker*	_fidfinder)
{
	fidfinder = _fidfinder;
}
void BlobTracker::doFiducialCalculation()
{	
	for(list<ofxFiducial>::iterator fiducial = fidfinder->fiducialsList.begin();fiducial!=fidfinder->fiducialsList.end();++fiducial)
	{
		fiducial->x_pos = fiducial->getX();
		fiducial->y_pos = fiducial->getY();
		fiducial->x_pos/=camWidth;
		fiducial->y_pos/=camHeight;
	}
}

BlobTracker::~BlobTracker()
{
}


//assigns IDs to each blob in the contourFinder
void BlobTracker::track(ContourFinder* newBlobs)
{
/*********************************************************************
//Object tracking
*********************************************************************/

	for(std::map<int,Blob>::iterator tracked=trackedObjects.begin();tracked != trackedObjects.end(); ++tracked)
	{//iterate through the tracked blobs and check if any of these are present in current blob list, else delete it (mark id = -1)
		int id= tracked->second.id;
		bool isFound=false; // The variable to check if the blob is in the new blobs list or not
		for(std::vector<Blob>::iterator blob = newBlobs->objects.begin();blob!=newBlobs->objects.end();++blob)
		{
			if(blob->id == id)
			{
				isFound=true;
				break;
			}
		}

		if(!isFound) // current tracked blob was not found in the new blob list
		{
			trackedObjects[id].id = -1;
		}
	}
	//handle the object tracking if present
	for (int i = 0; i < newBlobs->nObjects; i++)
	{
		int ID = newBlobs->objects[i].id;

		int now = ofGetElapsedTimeMillis();

		if(trackedObjects[ID].id == -1) //If this blob has appeared in the current frame
		{
			calibratedObjects[i]=newBlobs->objects[i];

			calibratedObjects[i].D.x=0;
			calibratedObjects[i].D.y=0;
			calibratedObjects[i].maccel=0;
			calibratedObjects[i].A = 0;
			calibratedObjects[i].raccel = 0;
			
			
			//Camera to Screen Position Conversion
			calibratedObjects[i].centroid.x/=camWidth;
			calibratedObjects[i].centroid.y/=camHeight;
			calibratedObjects[i].angleBoundingRect.width/=camWidth;
			calibratedObjects[i].angleBoundingRect.height/=camHeight;
		}
		else //Do all the calculations
		{
			float xOld = trackedObjects[ID].centroid.x;
			float yOld = trackedObjects[ID].centroid.y;

			float xNew = newBlobs->objects[i].centroid.x;
			float yNew = newBlobs->objects[i].centroid.y;

			//converting xNew and yNew to calibrated ones
			xNew/=camWidth;
			yNew/=camHeight;

			double dx = xNew-xOld;
			double dy = yNew-yOld;

			calibratedObjects[i] = newBlobs->objects[i];
			calibratedObjects[i].D.x = dx;
			calibratedObjects[i].D.y = dy;

			calibratedObjects[i].maccel = sqrtf((dx*dx+dy*dy)/(now - trackedObjects[ID].lastTimeTimeWasChecked));
			calibratedObjects[i].A = (((calibratedObjects[i].angle - trackedObjects[ID].angle)/(2*PI))/(now - trackedObjects[ID].lastTimeTimeWasChecked));
			calibratedObjects[i].raccel = (calibratedObjects[i].A - trackedObjects[ID].A)/(now - trackedObjects[ID].lastTimeTimeWasChecked);
	
			calibratedObjects[i].centroid.x/=camWidth;
			calibratedObjects[i].centroid.y/=camHeight;
			calibratedObjects[i].angleBoundingRect.width/=camWidth;
			calibratedObjects[i].angleBoundingRect.height/=camHeight;
		}
		trackedObjects[ID] = newBlobs->objects[i];
		trackedObjects[ID].lastTimeTimeWasChecked = now;
		trackedObjects[ID].centroid.x = calibratedObjects[i].centroid.x;
		trackedObjects[ID].centroid.y = calibratedObjects[i].centroid.y;
	}


/****************************************************************************
	//Finger tracking
****************************************************************************/

	//initialize ID's of all blobs
	for(int i=0; i<newBlobs->nBlobs; i++)
			newBlobs->blobs[i].id=-1;

	// STEP 1: Blob matching
	//
	//go through all tracked blobs to compute nearest new point
	for(int i = 0; i < trackedBlobs.size(); i++)
	{
		/******************************************************************
		 * *****************TRACKING FUNCTION TO BE USED*******************
		 * Replace 'trackKnn(...)' with any function that will take the
		 * current track and find the corresponding track in the newBlobs
		 * 'winner' should contain the index of the found blob or '-1' if
		 * there was no corresponding blob
		 *****************************************************************/
		int winner = trackKnn(newBlobs, &(trackedBlobs[i]), 3, 0);

		if(winner == -1) //track has died, mark it for deletion
		{
			//SEND BLOB OFF EVENT
			TouchEvents.messenger = trackedBlobs[i];

			if(isCalibrating)
			{
				TouchEvents.RAWmessenger = trackedBlobs[i];
				TouchEvents.notifyRAWTouchUp(NULL);
			}
			TouchEvents.messenger.boundingRect.width/=camWidth;
			TouchEvents.messenger.boundingRect.height/=camHeight;
			TouchEvents.messenger.centroid.x/=camWidth;
			TouchEvents.messenger.centroid.y/=camHeight;
			//erase calibrated blob from map
			calibratedBlobs.erase(TouchEvents.messenger.id);

			TouchEvents.notifyTouchUp(NULL);
			//mark the blob for deletion
			trackedBlobs[i].id = -1;
		}
		else //still alive, have to update
		{
			//if winning new blob was labeled winner by another track\
			//then compare with this track to see which is closer
			if(newBlobs->blobs[winner].id!=-1)
			{
				//find the currently assigned blob
				int j; //j will be the index of it
				for(j=0; j<trackedBlobs.size(); j++)
				{
					if(trackedBlobs[j].id==newBlobs->blobs[winner].id)
						break;
				}

				if(j==trackedBlobs.size())//got to end without finding it
				{
					newBlobs->blobs[winner].id = trackedBlobs[i].id;
					newBlobs->blobs[winner].age = trackedBlobs[i].age;
					newBlobs->blobs[winner].sitting = trackedBlobs[i].sitting;
					newBlobs->blobs[winner].downTime = trackedBlobs[i].downTime;
					newBlobs->blobs[winner].color = trackedBlobs[i].color;
					newBlobs->blobs[winner].lastTimeTimeWasChecked = trackedBlobs[i].lastTimeTimeWasChecked;

					trackedBlobs[i] = newBlobs->blobs[winner];
				}
				else //found it, compare with current blob
				{
					double x = newBlobs->blobs[winner].centroid.x;
					double y = newBlobs->blobs[winner].centroid.y;
					double xOld = trackedBlobs[j].centroid.x;
					double yOld = trackedBlobs[j].centroid.y;
					double xNew = trackedBlobs[i].centroid.x;
					double yNew = trackedBlobs[i].centroid.y;
					double distOld = (x-xOld)*(x-xOld)+(y-yOld)*(y-yOld);
					double distNew = (x-xNew)*(x-xNew)+(y-yNew)*(y-yNew);

					//if this track is closer, update the ID of the blob
					//otherwise delete this track.. it's dead
					if(distNew<distOld) //update
					{
						newBlobs->blobs[winner].id = trackedBlobs[i].id;
						newBlobs->blobs[winner].age = trackedBlobs[i].age;
						newBlobs->blobs[winner].sitting = trackedBlobs[i].sitting;
						newBlobs->blobs[winner].downTime = trackedBlobs[i].downTime;
						newBlobs->blobs[winner].color = trackedBlobs[i].color;
						newBlobs->blobs[winner].lastTimeTimeWasChecked = trackedBlobs[i].lastTimeTimeWasChecked;

//TODO--------------------------------------------------------------------------
						//now the old winning blob has lost the win.
						//I should also probably go through all the newBlobs
						//at the end of this loop and if there are ones without
						//any winning matches, check if they are close to this
						//one. Right now I'm not doing that to prevent a
						//recursive mess. It'll just be a new track.

						//SEND BLOB OFF EVENT
						TouchEvents.messenger = trackedBlobs[j];

						if(isCalibrating)
						{
							TouchEvents.RAWmessenger = trackedBlobs[j];
							TouchEvents.notifyRAWTouchUp(NULL);
						}
						TouchEvents.messenger.boundingRect.width/=camWidth;
						TouchEvents.messenger.boundingRect.height/=camHeight;
						TouchEvents.messenger.centroid.x/=camWidth;
						TouchEvents.messenger.centroid.y/=camHeight;

						//erase calibrated blob from map
						calibratedBlobs.erase(TouchEvents.messenger.id);

     					TouchEvents.notifyTouchUp(NULL);
						//mark the blob for deletion
						trackedBlobs[j].id = -1;
//------------------------------------------------------------------------------
					}
					else //delete
					{
						//SEND BLOB OFF EVENT
						TouchEvents.messenger = trackedBlobs[i];

						if(isCalibrating)
						{
							TouchEvents.RAWmessenger = trackedBlobs[i];
							TouchEvents.notifyRAWTouchUp(NULL);
						}
						TouchEvents.messenger.boundingRect.width/=camWidth;
						TouchEvents.messenger.boundingRect.height/=camHeight;
						TouchEvents.messenger.centroid.x/=camWidth;
						TouchEvents.messenger.centroid.y/=camHeight;
						//erase calibrated blob from map
						calibratedBlobs.erase(TouchEvents.messenger.id);

						TouchEvents.notifyTouchUp(NULL);
						//mark the blob for deletion
						trackedBlobs[i].id = -1;
					}
				}
			}
			else //no conflicts, so simply update
			{
				newBlobs->blobs[winner].id = trackedBlobs[i].id;
				newBlobs->blobs[winner].age = trackedBlobs[i].age;
				newBlobs->blobs[winner].sitting = trackedBlobs[i].sitting;
				newBlobs->blobs[winner].downTime = trackedBlobs[i].downTime;
				newBlobs->blobs[winner].color = trackedBlobs[i].color;
				newBlobs->blobs[winner].lastTimeTimeWasChecked = trackedBlobs[i].lastTimeTimeWasChecked;
			}
		}
	}

	// AlexP
	// save the current time since we will be using it a lot
	int now = ofGetElapsedTimeMillis();

	// STEP 2: Blob update
	//
	//--Update All Current Tracks
	//remove every track labeled as dead (ID='-1')
	//find every track that's alive and copy it's data from newBlobs
	for(int i = 0; i < trackedBlobs.size(); i++)
	{
		if(trackedBlobs[i].id == -1) //dead
		{
			//erase track
			trackedBlobs.erase(trackedBlobs.begin()+i, trackedBlobs.begin()+i+1);
			i--; //decrement one since we removed an element
		}
		else //living, so update it's data
		{
			for(int j = 0; j < newBlobs->nBlobs; j++)
			{
				if(trackedBlobs[i].id == newBlobs->blobs[j].id)
				{
					//update track
					ofPoint tempLastCentroid = trackedBlobs[i].centroid; // assign the new centroid to the old
					trackedBlobs[i] = newBlobs->blobs[j];
					trackedBlobs[i].lastCentroid = tempLastCentroid;

					ofPoint tD;
					//get the Differences in position
					tD.set(trackedBlobs[i].centroid.x - trackedBlobs[i].lastCentroid.x, 
							trackedBlobs[i].centroid.y - trackedBlobs[i].lastCentroid.y);
					//calculate the acceleration
					float posDelta = sqrtf((tD.x*tD.x)+(tD.y*tD.y));

					// AlexP
					// now, filter the blob position based on MOVEMENT_FILTERING value
					// the MOVEMENT_FILTERING ranges [0,15] so we will have that many filtering steps
					// Here we have a weighted low-pass filter
					// adaptively adjust the blob position filtering strength based on blob movement
					// http://www.wolframalpha.com/input/?i=plot+1/exp(x/15)+and+1/exp(x/10)+and+1/exp(x/5)+from+0+to+100
					float a = 1.0f - 1.0f / expf(posDelta / (1.0f + (float)MOVEMENT_FILTERING*10));
					trackedBlobs[i].centroid.x = a * trackedBlobs[i].centroid.x + (1-a) * trackedBlobs[i].lastCentroid.x;
					trackedBlobs[i].centroid.y = a * trackedBlobs[i].centroid.y + (1-a) * trackedBlobs[i].lastCentroid.y;

					//get the Differences in position
					trackedBlobs[i].D.set(trackedBlobs[i].centroid.x - trackedBlobs[i].lastCentroid.x, 
											trackedBlobs[i].centroid.y - trackedBlobs[i].lastCentroid.y);

					//if( abs((int)trackedBlobs[i].D.x) > 1 || abs((int)trackedBlobs[i].D.y) > 1) {
//						printf("\nUNUSUAL BLOB @ %f\n-----------------------\ntrackedBlobs[%i]\nD = (%f, %f)\nXY= (%f, %f)\nlastTimeTimeWasChecked = %f\nsitting = %f\n",
//							   ofGetElapsedTimeMillis(),
//							   i,
//							   trackedBlobs[i].D.x,  trackedBlobs[i].D.y,
//							   trackedBlobs[i].centroid.x, trackedBlobs[i].centroid.y,
//							   trackedBlobs[i].lastTimeTimeWasChecked,
//							   trackedBlobs[i].downTime,
//							   trackedBlobs[i].sitting
//						);
//					}

					//calculate the acceleration again
					tD = trackedBlobs[i].D;
					trackedBlobs[i].maccel = sqrtf((tD.x* tD.x)+(tD.y*tD.y)) / (now - trackedBlobs[i].lastTimeTimeWasChecked);

					//calculate the age
					trackedBlobs[i].age = ofGetElapsedTimef() - trackedBlobs[i].downTime;

					//set sitting (held length)
                    if(trackedBlobs[i].maccel < 7)
					{	//1 more frame of sitting
						if(trackedBlobs[i].sitting != -1)
							trackedBlobs[i].sitting = ofGetElapsedTimef() - trackedBlobs[i].downTime;           
					}
					else
						trackedBlobs[i].sitting = -1;

					//printf("time: %f\n", ofGetElapsedTimeMillis());
					//printf("%i age: %f, downTimed at: %f\n", i, trackedBlobs[i].age, trackedBlobs[i].downTime);

					//if blob has been 'holding/sitting' for 1 second send a held event
					if(trackedBlobs[i].sitting > 1.0f)
					{
						//SEND BLOB HELD EVENT
						TouchEvents.messenger = trackedBlobs[i];

						if(isCalibrating)
						{
							TouchEvents.RAWmessenger = trackedBlobs[i];
							TouchEvents.notifyRAWTouchHeld(NULL);
						}

						//calibrated values
						TouchEvents.messenger.boundingRect.width/=camWidth;
						TouchEvents.messenger.boundingRect.height/=camHeight;
						TouchEvents.messenger.centroid.x/=camWidth;
						TouchEvents.messenger.centroid.y/=camHeight;
						TouchEvents.messenger.lastCentroid.x/=camWidth;
						TouchEvents.messenger.lastCentroid.y/=camHeight;
						
						//Calibrated dx/dy
						TouchEvents.messenger.D.set(trackedBlobs[i].centroid.x - trackedBlobs[i].lastCentroid.x, 
												trackedBlobs[i].centroid.y - trackedBlobs[i].lastCentroid.y);

						//calibrated acceleration
						ofPoint tD = TouchEvents.messenger.D;
						TouchEvents.messenger.maccel = sqrtf((tD.x*tD.x)+(tD.y*tD.y)) / (now - TouchEvents.messenger.lastTimeTimeWasChecked);
						TouchEvents.messenger.lastTimeTimeWasChecked = now;

						//add to calibration map
						calibratedBlobs[TouchEvents.messenger.id] = TouchEvents.messenger;

                        //held event only happens once so set to -1
                        trackedBlobs[i].sitting = -1;

						TouchEvents.notifyTouchHeld(NULL);
					} 
					else 
					{
						//printf("(%f, %f) -> (%f, %f) \n", trackedBlobs[i].lastCentroid.x, trackedBlobs[i].lastCentroid.y, trackedBlobs[i].centroid.x, trackedBlobs[i].centroid.y);

						//SEND BLOB MOVED EVENT
						TouchEvents.messenger = trackedBlobs[i];

						if(isCalibrating)
						{
							TouchEvents.RAWmessenger = trackedBlobs[i];
							TouchEvents.notifyRAWTouchMoved(NULL);
						}

						//calibrated values
						TouchEvents.messenger.boundingRect.width/=camWidth;
						TouchEvents.messenger.boundingRect.height/=camHeight;

						TouchEvents.messenger.centroid.x/=camWidth;
						TouchEvents.messenger.centroid.y/=camHeight;

						TouchEvents.messenger.lastCentroid.x/=camWidth;
						TouchEvents.messenger.lastCentroid.y/=camHeight;

						//Calibrated dx/dy
						TouchEvents.messenger.D.set(trackedBlobs[i].centroid.x - trackedBlobs[i].lastCentroid.x, 
												trackedBlobs[i].centroid.y - trackedBlobs[i].lastCentroid.y);

						//printf("d(%0.4f, %0.4f)\n", TouchEvents.messenger.D.x, TouchEvents.messenger.D.y);
				
						//calibrated acceleration
						ofPoint tD = TouchEvents.messenger.D;
						TouchEvents.messenger.maccel = sqrtf((tD.x*tD.x)+(tD.y*tD.y)) / (now - TouchEvents.messenger.lastTimeTimeWasChecked);
						TouchEvents.messenger.lastTimeTimeWasChecked = now;

						//add to calibration map
						calibratedBlobs[TouchEvents.messenger.id] = TouchEvents.messenger;

						TouchEvents.notifyTouchMoved(NULL);
					}
					// AlexP
					// The last lastTimeTimeWasChecked is updated at the end after all acceleration values are calculated
					trackedBlobs[i].lastTimeTimeWasChecked = now;
				}
			}
		}
	}

	// STEP 3: add tracked blobs to TouchEvents
	//--Add New Living Tracks
	//now every new blob should be either labeled with a tracked ID or\
	//have ID of -1... if the ID is -1... we need to make a new track
	for(int i = 0; i < newBlobs->nBlobs; i++)
	{
		if(newBlobs->blobs[i].id==-1)
		{
			//add new track
			newBlobs->blobs[i].id=IDCounter++;
			newBlobs->blobs[i].downTime = ofGetElapsedTimef();
			//newBlobs->blobs[i].lastTimeTimeWasChecked = ofGetElapsedTimeMillis();

			//random color for blob. Could be useful?
			int r = ofRandom(0, 255);
            int g = ofRandom(0, 255);
            int b = ofRandom(0, 255);
            //Convert to hex
            int rgbNum = ((r & 0xff) << 16) | ((g & 0xff) << 8) | (b & 0xff);
            //Set color
            newBlobs->blobs[i].color = rgbNum;

			//Add to blob messenger
			TouchEvents.messenger = newBlobs->blobs[i];

			if(isCalibrating)
			{
				TouchEvents.RAWmessenger = newBlobs->blobs[i];
				TouchEvents.notifyRAWTouchDown(NULL);
			}
			TouchEvents.messenger.boundingRect.width/=camWidth;
			TouchEvents.messenger.boundingRect.height/=camHeight;      

			TouchEvents.messenger.centroid.x/=camWidth;
			TouchEvents.messenger.centroid.y/=camHeight;

			//add to calibrated blob map
			calibratedBlobs[TouchEvents.messenger.id] = TouchEvents.messenger;

			//Send Event
			TouchEvents.notifyTouchDown(NULL);
			trackedBlobs.push_back(newBlobs->blobs[i]);
		}
	}
}

std::map<int, Blob> BlobTracker::getTrackedBlobs()
{
    return calibratedBlobs;
}

std::map<int, Blob> BlobTracker::getTrackedObjects()
{
	return calibratedObjects;
}

/*************************************************************************
* Finds the blob in 'newBlobs' that is closest to the trackedBlob with index
* 'ind' according to the KNN algorithm and returns the index of the winner
* newBlobs	= list of blobs detected in the latest frame
* track		= current tracked blob being tested
* k			= number of nearest neighbors to consider\
*			  1,3,or 5 are common numbers..\
*			  must always be an odd number to avoid tying
* thresh	= threshold for optimization
**************************************************************************/
int BlobTracker::trackKnn(ContourFinder *newBlobs, Blob *track, int k, double thresh = 0)
{

	int winner = -1; //initially label track as '-1'=dead
	if((k%2)==0) k++; //if k is not an odd number, add 1 to it

	//if it exists, square the threshold to use as square distance
	if(thresh>0)
		thresh *= thresh;

	//list of neighbor point index and respective distances
	std::list<std::pair<int,double> > nbors;
	std::list<std::pair<int,double> >::iterator iter;

	//find 'k' closest neighbors of testpoint
	double x, y, xT, yT, dist;
	for(int i=0; i<newBlobs->nBlobs; i++)
	{
		x = newBlobs->blobs[i].centroid.x;
		y = newBlobs->blobs[i].centroid.y;

		xT = track->centroid.x;
		yT = track->centroid.y;
		dist = (x-xT)*(x-xT)+(y-yT)*(y-yT);

		if(dist<=thresh)//it's good, apply label if no label yet and return
		{
			winner = i;
			return winner;
		}

		/****************************************************************
		* check if this blob is closer to the point than what we've seen
		*so far and add it to the index/distance list if positive
		****************************************************************/

		//search the list for the first point with a longer distance
		for(iter=nbors.begin(); iter!=nbors.end()
			&& dist>=iter->second; iter++);

		if((iter!=nbors.end())||(nbors.size()<k)) //it's valid, insert it
		{
			nbors.insert(iter, 1, std::pair<int, double>(i, dist));
			//too many items in list, get rid of farthest neighbor
			if(nbors.size()>k)
				nbors.pop_back();
		}
	}

	/********************************************************************
	* we now have k nearest neighbors who cast a vote, and the majority
	* wins. we use each class average distance to the target to break any
	* possible ties.
	*********************************************************************/

	// a mapping from labels (IDs) to count/distance
	std::map<int, std::pair<int, double> > votes;

	//remember:
	//iter->first = index of newBlob
	//iter->second = distance of newBlob to current tracked blob
	for(iter=nbors.begin(); iter!=nbors.end(); iter++)
	{
		//add up how many counts each neighbor got
		int count = ++(votes[iter->first].first);
		double dist = (votes[iter->first].second+=iter->second);

		/* check for a possible tie and break with distance */
		if(count>votes[winner].first || count==votes[winner].first
			&& dist<votes[winner].second)
		{
			winner = iter->first;
		}
	}
	return winner;
}
