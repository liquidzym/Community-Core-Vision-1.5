/*
*  Calibration.h
*  
*
*  Created on 2/2/09.
*  Copyright 2009 NUI Group. All rights reserved.
*
*/

#ifndef CALIBRATION_H
#define CALIBRATION_H

#include "ofMain.h"
#include "CalibrationUtils.h"
#include "../Events/TouchMessenger.h"
#include "../Tracking/ContourFinder.h"
#include "../Tracking/Tracking.h"

class Calibration : public TouchListener {

	public:
		Calibration();

		//Basic Methods
		void setup(unsigned int stitchedFrameWidth,unsigned int stitchedFrameHeight,unsigned int cameraGridWidth,unsigned int cameraGridHeight,unsigned int calibrationGridWidth,unsigned int calibrationGridHeight,bool isInterleaveMode);
		void SetTracker(BlobTracker *trackerIn);
		//Key Events
		void _keyPressed(ofKeyEventArgs &e);
		void _keyReleased(ofKeyEventArgs &e);
        //Touch Events
		void RAWTouchDown( Blob b );
		void RAWTouchMoved( Blob b );
		void RAWTouchUp( Blob b );
		void RAWTouchHeld( Blob b);
		//other
        void doCalibration();
		void passInContourFinder(int numBlobs, vector<Blob> blobs);
        void passInTracker(BlobTracker *trackerIn);
		bool calibrating;
		bool shouldStart;		
		static Calibration* getInstance();
		CalibrationUtils calibrate;
	private:	
		void drawFingerOutlines();
		void drawCalibrationBlobs();
		void drawCalibrationPointsAndBox();
		void saveConfiguration();
        void DrawCircleLoader(double xctr, double yctr, double radius, double startAngle, double endAngle);
        bool			bW;
		bool			bS;
		bool			bA;
		bool			bD;
		bool			bShowTargets;
        int 			camWidth;
		int 			camHeight;
		float           arcAngle;
		float			targetColor;
		//Fonts
		ofTrueTypeFont	verdana;
		ofTrueTypeFont	calibrationText;
		//Draw Particle Image
		ofImage			calibrationParticle;		
		//Blob Tracker
		BlobTracker*	tracker;
		ContourFinder	contourFinder;
};

#endif
