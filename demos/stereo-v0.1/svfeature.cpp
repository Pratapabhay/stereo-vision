/**
 * @file   svfeature.cpp
 * @author Liangfu Chen <chenclf@gmail.com>
 * @date   Tue Feb  7 17:02:55 2012
 * 
 * @brief  
 * 
 * 
 */

#include "svfeature.h"
#include "svcalib.h"
#include "svrectify.h"

#include "svutility.h"

const int MAX_CORNERS = 500;

void extractFeatureSURF(CvArr * pImage, bool saveImage)
{
	IplImage * pWorkImage = cvCreateImage(cvGetSize(pImage),IPL_DEPTH_8U,1);

	// cvCvtColor(pImage,pWorkImage,CV_BGR2GRAY);
	pWorkImage = cvCloneImage((IplImage*)pImage);

	CvMemStorage* storage = cvCreateMemStorage(0);
	CvSeq *imageKeypoints = 0, *imageDescriptors = 0;
	CvSURFParams params = cvSURFParams(2000, 0);
	cvExtractSURF( pWorkImage, 0, &imageKeypoints, &imageDescriptors,
				   storage, params );
	// show features
	for( int i = 0; i < imageKeypoints->total; i++ )
	{
		CvSURFPoint* r = (CvSURFPoint*)cvGetSeqElem( imageKeypoints, i );
		CvPoint center;
		int radius;
		center.x = cvRound(r->pt.x);
		center.y = cvRound(r->pt.y);
		radius = cvRound(r->size*1.2/9.*2);
		cvCircle( pWorkImage, center, radius, CV_RGB(255,0,0), 1, CV_AA, 0 );
	}
	// if (saveImage) // svShowImage(pWorkImage);
	// {cvSaveImage("feature.pgm", pWorkImage);}
	
	cvReleaseImage(&pWorkImage);
	cvReleaseMemStorage(&storage);
}

bool extractFeatureKLT(const CvArr * imgA, const CvArr * imgB,
					   const bool saveImage)
{
	// Load two images and allocate other structures
	// IplImage* imgA = cvLoadImage("image0.png", CV_LOAD_IMAGE_GRAYSCALE);
	// IplImage* imgB = cvLoadImage("image1.png", CV_LOAD_IMAGE_GRAYSCALE);

	CvSize imageSize = cvGetSize( imgA );
	const int win_size = 15;

	IplImage* imgC = cvCloneImage((IplImage*)imgB);
	float alpha = 0.5f;
	// cvAddWeighted : dst(I)=src1(I)*alpha+src2(I)*beta+gamma
	cvAddWeighted(imgC, alpha, imgA, 1.0f-alpha, 0.0, imgC);

	// Get the features for tracking
	IplImage* eig_image = cvCreateImage( imageSize, IPL_DEPTH_32F, 1 );
	IplImage* tmp_image = cvCreateImage( imageSize, IPL_DEPTH_32F, 1 );

	int corner_count = MAX_CORNERS;
	CvPoint2D32f* cornersA = new CvPoint2D32f[ MAX_CORNERS ];

	cvGoodFeaturesToTrack( imgA, eig_image, tmp_image, cornersA, 
						   &corner_count,
						   0.05, 5.0, 0, 
						   3/* block_size */, 0/* use_harris */, 0.04 );

	/* cvScale(eig_image, eig_image, 100, 0.00); */
	/* svShowImage(eig_image); */

	cvFindCornerSubPix( imgA, cornersA, corner_count, 
						cvSize( win_size, win_size ),
						cvSize( -1, -1 ), 
						cvTermCriteria( CV_TERMCRIT_ITER | 
										CV_TERMCRIT_EPS, 20, 0.03 ) );

	// Call Lucas Kanade algorithm
	char features_found[ MAX_CORNERS ];
	float feature_errors[ MAX_CORNERS ];

	// CvSize pyr_sz = cvSize( imgA->width+8, imgB->height/3 );
	CvSize pyr_sz = cvSize( cvGetSize(imgA).width+8,
							cvGetSize(imgB).height/3 );

	IplImage* pyrA = cvCreateImage( pyr_sz, IPL_DEPTH_32F, 1 );
	IplImage* pyrB = cvCreateImage( pyr_sz, IPL_DEPTH_32F, 1 );

	CvPoint2D32f* cornersB = new CvPoint2D32f[ MAX_CORNERS ];

	// cvTermCriteria(
	// 			   int    type,
	// 			   int    max_iter,
	// 			   double epsilon
	// 			   );
	// Actually run Pyramidal Lucas Kanade Optical Flow!!
	cvCalcOpticalFlowPyrLK( imgA, imgB, pyrA, pyrB, 
							cornersA, cornersB, corner_count, 
							cvSize( win_size, win_size ),
							5/* maximum number of pyramids */, 
							features_found, feature_errors,
							// this criteria is important to
							// control the result
							cvTermCriteria( CV_TERMCRIT_ITER | 
											CV_TERMCRIT_EPS, 20// 20
											, 0.3// 0.3
											),
							0 /* enhancements */
							/* CV_LKFLOW_PYR_B_READY | 
							   CV_LKFLOW_PYR_A_READY |
							   CV_LKFLOW_INITIAL_GUESSES */ );

	// Make an image of the results
	// imgC->origin = ((IplImage*)imgB)->origin;

    // vector<CvPoint3D32f> objectPoints;
    // vector<CvPoint2D32f> points[2];
	vector<CvPoint> points[2];

	for( int k,i = k = 0; i < corner_count; i++ )
	{
		if ( !features_found[i] ){ continue; }
		if ( feature_errors[i]>950 ){
			//fprintf(stderr, "", feature_errors[i]);
			continue;
		}
                
		// cornersB[k++] = cornersB[i];
		// cvCircle( imgC, cvPointFrom32f(cornersB[i]),
		// 		  3, CV_RGB(0,255,0), -1, 8, 0);
		CvPoint p0 = cvPoint( cvRound( cornersA[i].x ),
							  cvRound( cornersA[i].y ) );
		CvPoint p1 = cvPoint( cvRound( cornersB[i].x ),
							  cvRound( cornersB[i].y ) );
		points[0].push_back(p0);
		points[1].push_back(p1);

		// draw line between all match points
		cvLine( imgC, p0, p1, CV_RGB(255,0,0), 1 );
	}

	int N = points[0].size();

	assert(points[0].size()==points[0].size());

	// estimate fundamental matrix F by point matches found
    CvMat _imagePoints1 = cvMat(1, N, CV_32FC2, &points[0][0] );
    CvMat _imagePoints2 = cvMat(1, N, CV_32FC2, &points[1][0] );
    // _imagePoints1 = cvMat(1, N, CV_32FC2, &points[0][0] );
    // _imagePoints2 = cvMat(1, N, CV_32FC2, &points[1][0] );

    double F[3][3];
	double H1[3][3], H2[3][3]/* , iM[3][3] */;
	CvMat _H1 = cvMat(3, 3, CV_64F, H1);
	CvMat _H2 = cvMat(3, 3, CV_64F, H2);
    CvMat _F = cvMat(3, 3, CV_64F, F );
	// CvMat _iM = cvMat(3, 3, CV_64F, iM);
	CvMat * F_status = cvCreateMat(1, points[0].size(), CV_8UC1);

	cvFindFundamentalMat( &_imagePoints1, &_imagePoints2, &_F,
						  CV_FM_LMEDS// CV_FM_RANSAC
						  ,
						  0.5, 0.2, // 1.0, 0.99,
						  F_status
						  /* use RANSAC by default */);
	// if (saveImage) {svShowImage(imgC);}

    CvMat * H_status = cvCreateMat(1, points[0].size(), CV_8UC1);//cvMat(1, N, CV_32FC2, &points[1][0] );
	cvFindHomography( &_imagePoints1,
					  &_imagePoints2, &_H1,
					  // CV_LMEDS, 3, _status
					  CV_RANSAC, 3, H_status
					  );
	
	imgC = cvCloneImage((IplImage*)imgB);
	cvAddWeighted(imgC, alpha, imgA, 1.0f-alpha, 0.0, imgC);

	// svStereoRectifyUncalibrated( &_imagePoints1,
	// 							 &_imagePoints2, &_F,
	// 							 imageSize,
	// 							 &_H1, &_H2, 3/* threshold */);

	// cvInvert(&_M1, &_iM);
	// cvMatMul(&_H1, &_M1, &_R1);
	// cvMatMul(&_iM, &_R1, &_R1);
	// cvInvert(&_M2, &_iM);
	// cvMatMul(&_H2, &_M2, &_R2);
	// cvMatMul(&_iM, &_R2, &_R2);

	cvSave("tmp/F.xml", &_F);
	cvSave("tmp/H1.xml", &_H1);
	cvSave("tmp/F_status.xml", F_status);
	cvSave("tmp/H_status.xml", H_status);
	// cvSave("tmp/H2.xml", &_H2);
	fprintf(stderr, "N: %d\n", N);

	//if (saveImage) {svShowImage(imgC);}
	return 0;
}
