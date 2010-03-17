#include <Engine3D.h>

#ifndef __WIN32__
#include <sys/time.h>
#endif

Engine3D::Engine3D(QWidget *parent, char *name)
{

    this->cameraLocator =  gluNewQuadric();
    this->setUpSky();
    this->fadeSpeed = 20;
    this->setMouseTracking(true);
    this->setAutoFillBackground(false);
    this->cameraPosition.x = 0.0;
    this->cameraPosition.y = 0.0;
    this->cameraPosition.z = 0.0;
    this->cameraPosition.w = 1.0;
    this->zoom = 1.0;
    this->maxPitch = 45.0; //[-45,45]
    this->currentPitch = 0.0;
	//search for bars by default
    this->searchDestinationType = E_BAR;
    this->speed = 1.0;


    this->pathCounter = 0;
    this->pathLength =  36;

    this->lightsOn = false;
    this->numlights = 10;


    this->fnear = 1.0F;
    this->ffar = 2000.0F;

    this->collisionPadding = this->fnear+1.0;



    this->currDestination = NULL;

    MouseActive = false;

#ifdef __WIN32__
    textureLoader = new TextureLoader;
#endif

    this->colladaLoader = NULL;
    this->colladaAuxObjects = NULL;

    this->isMakeScreenShot = false;

#ifdef __WIN32__
    //register AccData structure for Slots/Signals mechanism
    qRegisterMetaType<AccData>("AccData");

    //initialize Shake SK7 pointers
    this->bt = new QBtooth(13);
    this->bThr = new QBthread(this->bt);
#endif

    this->routeGraph = new RouteGraph;

    this->inInsideView = false;

    this->fadeAlphaLevel = 0;

    this->applicationState = E_3DVIEW;

#ifdef __WIN32__
    //connect Engine3D to QBtooth
    QObject::connect(this->bt, SIGNAL(dataReceived(AccData)), this, SLOT(printAccData(AccData)));
    QObject::connect(this->bt, SIGNAL(disconnected(bool)), this, SLOT(isDisconnected(bool)));


    this->selectedVideoWidget = -1;
#endif

    this->selectedWidget = -1;

#ifdef __WIN32_
    this->isVideoPlaying = false;
    _
    //start thread
    #warning "UNCOMMENT this->bThr->start() to use shaker SK7"
    this->bThr->start();
#endif

    //widgets for Widgets3D
    this->iw = NULL;
    this->iw3 = NULL;
    this->selector = E_FILTER;
    this->popup = new Popup;


    this->frustumIndex = 1.0;
    this->frames = 0;

}

void Engine3D::setUpSky()
{
    this->sky = gluNewQuadric();
    gluQuadricTexture(this->sky,GL_TRUE );
    gluQuadricNormals(this->sky,GLU_SMOOTH);
    gluQuadricDrawStyle( this->sky, GLU_FILL);

    gluQuadricOrientation(this->sky, GLU_INSIDE);
    this->skyangle = 0;
}

void Engine3D::drawSky()
{
    this->skyangle+=0.1;
    glEnable(GL_TEXTURE_2D);
    glBindTexture(GL_TEXTURE_2D, this->skytexture.TextureID);

    glPushMatrix();

        glTranslated(0,-200,-600);
        //so the texture seams are not visible
        glRotatef(this->skyangle,0,1,0);
        glRotatef(180,0,0,1);
        float black[] = {0.0,0.0,0.0};
        GLfloat emissive[] = { 0.05, 0.01, 0.01, 1.0 };
        glColor3fv(black);
        glMaterialfv(GL_FRONT, GL_EMISSION, emissive);
        gluSphere(this->sky,1200,20,20);
    glPopMatrix();
    glDisable(GL_TEXTURE_2D);
}

void Engine3D::initializeGL()
{
    cout << "INITIALIZE GL" << endl;
    setAutoBufferSwap (true);

    glClearColor(0.529,0.807,0.98,1.0);
    //glClearColor(0,0,0,1);

    glShadeModel(GL_SMOOTH);
    glEnable(GL_DEPTH_TEST);
    glEnable(GL_CULL_FACE);
    glCullFace(GL_BACK); //default but just in case
    //glPolygonMode(GL_BACK, GL_LINE); //in case we wanted to draw something for the back faces


    this->menuManager = new MenuManager();
    QObject::connect(this->menuManager, SIGNAL(clicked(QString)), this, SLOT(menuItemClicked(QString)));
    this->resetCameraTransform();
    this->reset3DProjectionMatrix();
    this->resultManager = new ResultManager(this);

    this->distanceSelector = new DistanceSelector(this->size());
    this->optionsSelector = new OptionsSelector(this->size());
    this->pricesSelector = new PricesSelector(this->size());
    this->nameSelector = new NameSelector(this->size());
    this->filterSelector = new FilterSelector(this->size());
    this->srSelector = new SearchResSelector(this->size());
    this->turnLightsOn(this->lightsOn);
    if (loadScene("../res/treD.dae", "../res/auxobjects.dae"))

    {
            this->createDestinations();
            QSound::play("../res/sounds/wind.wav");
            m_timer = new QTimer( this );
            connect( m_timer, SIGNAL(timeout()), this, SLOT(timeOutSlot()) );
            m_timer->start(20);

            INT64 lGetTickCount;

#ifdef __WIN32__
            lGetTickCount = GetTickCount();
#else
            //get the current number of microseconds since january 1st 1970
            struct timeval ts;
            gettimeofday(&ts,0);
            lGetTickCount = (INT64)(ts.tv_sec * 1000 + (ts.tv_usec / 1000));
#endif

            this->startTime =  lGetTickCount;//GetTickCount();

            this->texture_timer = new QTimer(this);
            connect( texture_timer, SIGNAL(timeout()), this, SLOT(textureTimerSlot()) );
            texture_timer->start(40);
    }
    else
    {
            cout<<"error loading scene"<<endl;
    }

}

void Engine3D::reset3DProjectionMatrix()
{
    glMatrixMode(GL_PROJECTION);
    glPushMatrix();
        glLoadIdentity();
        glFrustum(this->fleft, this->fright, this->fbottom, this->ftop, this->fnear, this->ffar);
        glGetDoublev(GL_PROJECTION_MATRIX, this->proj_matrix);
    glPopMatrix();
}
void Engine3D::resetCameraTransform()
{
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
        glLoadIdentity();
        glGetDoublev(GL_MODELVIEW_MATRIX, rot_matrix);
    glPopMatrix();
}

void Engine3D::translateCamera(GLfloat x, GLfloat y, GLfloat z)
{
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
        glTranslatef(x, y, z);
        glGetDoublev(GL_MODELVIEW_MATRIX, rot_matrix);
    glPopMatrix();
}

void Engine3D::resetCameraRotations()
{
    double tx = rot_matrix[12];
    double ty = rot_matrix[13];
    double tz = rot_matrix[14];


    glPushMatrix();
        glLoadIdentity();
        glGetDoublev(GL_MODELVIEW_MATRIX, rot_matrix);
    glPopMatrix();

    rot_matrix[12] = tx;
    rot_matrix[13] = ty;
    rot_matrix[14] = tz;
}

void Engine3D::resizeGL( int width, int height )
{

    this->fright = this->frustumIndex;
    this->fleft = -this->fright;
    this->ftop = (float)height/width*this->fright;
    this->fbottom = -this->ftop;

    this->reset3DProjectionMatrix();

    this->screenw = width;
    this->screenh = height;
    this->menuManager->resize(this->screenw,this->screenh);
    this->popup->resize(width,height);
    this->resizeSelector(this->size());
}


//void Engine3D::paintGL()
void Engine3D::paintEvent(QPaintEvent *event)
{


    this->DrawCaca();

    this->frames++;

    INT64 lGetTickCount;
#ifdef __WIN32__
    lGetTickCount = GetTickCount();
#else
    //get the current number of microseconds since january 1st 1970
    struct timeval ts;
    gettimeofday(&ts,0);
    lGetTickCount = (INT64)(ts.tv_sec * 1000 + (ts.tv_usec / 1000));
#endif

    this->fps = (float)this->frames/(/*GetTickCount()*/lGetTickCount - startTime)*1000;
}

void Engine3D::DrawView3D()
{
    glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    DrawMainView();
    DrawSideBar();
    //DrawTopBar();
    glFlush ();
}

void Engine3D::DrawInsideView3D()
{
    glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    this->DrawInsideView();
    glFlush();
}

void Engine3D::DrawInsideView()
{
    glMatrixMode(GL_PROJECTION);
    glLoadMatrixd(this->proj_matrix);
    glViewport(0,0,(GLsizei)screenw, (GLsizei)screenh);

    glMatrixMode(GL_MODELVIEW);

    

    glPushMatrix();
        setCamera();
        this->illuminate();
        for(int i=0; i< numObjects; i++)
        {
            objects[i]->render();
        }

    glPopMatrix();
}

void Engine3D::DrawSearchView3D()
{
    glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
    this->DrawSearchView();
    glFlush ();
}

void Engine3D::DrawSearchView()
{
    glMatrixMode(GL_PROJECTION);
    glLoadMatrixd(this->proj_matrix);
    glViewport(0,0,(GLsizei)screenw, (GLsizei)screenh);

    glMatrixMode(GL_MODELVIEW);


    glPushMatrix();

        //cout << "Zoom: " << zoom;
        //cout << " Zoom ...: " << 500.0*(zoom-1.0) << endl;

        this->setSearchCamera();
        this->illuminate();
        //this->drawSky();

        for(int i=0; i< numObjects; i++)
        {
            objects[i]->render();
        }
    glPopMatrix();
}

void Engine3D::saveCameraTransform()
{
    glPushMatrix();
    glMultMatrixd(rot_matrix);
    glGetDoublev(GL_MODELVIEW_MATRIX,currpos_matrix);
    glPopMatrix();
}
void Engine3D::DrawCaca()
{
    switch(this->applicationState)
    {
        case E_3DVIEW:
            {
            if(!this->inInsideView)
                this->DrawView3D();
            else
                this->DrawInsideView3D();
            QPainter painter(this);
            this->renderMenu(&painter);

            if(this->currDestination && !this->inInsideView)
            {
                QPointF scrCoords =
                        this->getScreenCoords(
                         this->currDestination->coords);

                if(!scrCoords.isNull())
                {
                    this->popup->render(&painter,scrCoords,
                                        this->currDestination->name.c_str());
                }
            }
            else if(this->currObjInfo.info != "" && this->inInsideView)
            {
                QPointF scrCoords =
                        this->getScreenCoords(
                         this->currObjInfo.tailCoords);
                this->popup->render(&painter,scrCoords,
                                        this->currObjInfo.info);
            }
            painter.end();
            }
            break;
        case E_SELECTOR:
            {
            this->DrawSearchView3D();

            QPainter painter(this);            
            //this->distanceSelector->render(&painter);
            //this->optionsSelector->render(&painter);
            //this->pricesSelector->render(&painter);
            //this->nameSelector->render(&painter);
            this->renderMenu(&painter);

            switch (this->selector)
         	{
                case E_NAME:
                    this->nameSelector->render(&painter);
                    break;
                case E_FILTER:
                    this->filterSelector->render(&painter);
                    break;
                case E_DISTANCE:
                    this->distanceSelector->render(&painter);
                    break;
                case E_OPTIONS:
                    this->optionsSelector->render(&painter);
                    break;
                case E_PRICES:
                    this->pricesSelector->render(&painter);
                    break;
                case E_SR:
                    this->srSelector->render(&painter);
                    this->drawMapMarks(&painter);
                    break;
                default:
                    break;

            }
            this->updateSelector(&painter);
            painter.end();

            }
            break;

        case E_FADE_IN:
            {
                if(this->fadeAlphaLevel + this->fadeSpeed < 255)
                {
                    this->fadeAlphaLevel+=this->fadeSpeed;
                    this->DrawView3D();
                    QPainter painter(this);
                    this->DrawRectangle(0,0,0,this->fadeAlphaLevel, painter);
                    painter.end();

                }
                else
                {
                    this->fadeAlphaLevel = 255;
                    QPainter painter(this);
                    this->DrawRectangle(0,0,0,this->fadeAlphaLevel, painter);
                    painter.end();
                    
                    this->destroyWidgets3D();
					this->destroySceneObjects();
                    if(!this->inInsideView)
                    {
                        //save the current position
                        this->saveCameraTransform();
                        this->resetCameraTransform();
                        this->moveCameraRelative(0.0f, -2.0f, -1.5f);

                        if (this->loadScene("../res/insidebar.dae",
                                            "../res/auxobjects.dae"))
                        {
                            //create 3d widgets
                            this->createWidgets3D();
                            this->applicationState = E_FADE_OUT;
                            cout<<"going to fadeout"<<endl;
                        }
                        else
                        {
                            cout<<"couldnt load the scene... going to E_NOTHING"<<endl;
                            this->applicationState = E_NOTHING;
                        }
                        this->inInsideView = true;
                    }
                    else
                    {
                            if (this->loadScene("../res/treD.dae", "../res/auxobjects.dae"))
	                    {
	                        //create 3d widgets
                                this->restoreCameraPosition();

	                        this->applicationState = E_FADE_OUT;
	                        cout<<"going to fadeout"<<endl;
	                    }
	                    else
	                    {
	                        cout<<"couldnt load the scene... going to E_NOTHING"<<endl;
	                        this->applicationState = E_NOTHING;
	                    }				
	                    this->inInsideView = false;	
                    }

                }
            }
            break;

        case E_FADE_OUT:
            {
                this->DrawView3D();
                if(this->fadeAlphaLevel - this->fadeSpeed > 0)
                {
                    this->fadeAlphaLevel-=this->fadeSpeed;
                    QPainter painter(this);
                    this->DrawRectangle(0,0,0,this->fadeAlphaLevel, painter);
                    painter.end();
                }
                else
                {
                    this->fadeAlphaLevel = 0;
                    this->applicationState = E_3DVIEW;
                    cout<<"going back to E_3DVIEW"<<endl;

                    if(this->inInsideView)
                    {
                        QSound::play("../res/sounds/bar_noise.wav");
                    }
                }
            }
            break;

        case E_ASCENDING:
            if(this->pathCounter <= this->pathLength)
            {
                this->DrawAscending3D();
            }
            else
            {
                this->zoom = 1.0f;
                this->applicationState = E_SELECTOR;
            }
            break;

    case E_DESCENDING:
            if(this->pathCounter <= this->pathLength)
            {
                this->DrawAscending3D();
            }
            else
            {
                this->zoom = 1.0f;
                this->applicationState = E_3DVIEW;
                this->currentPitch = 0;
            }
            break;
       

        case E_NOTHING:
            break;

        default:
            break;
    }
}

void Engine3D::DrawAscending()
{
    glClear (GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);

    glMatrixMode(GL_PROJECTION);
    glLoadMatrixd(this->proj_matrix);
    glViewport(0,0,(GLsizei)screenw, (GLsizei)screenh);

    glMatrixMode(GL_MODELVIEW);


    glPushMatrix();
        setCamera();
        //this->updateFrustrumCoords();
        this->drawSky();
        this->illuminate();
        for(int i=0; i< numObjects; i++)
        {
            objects[i]->render();
        }
        this->DrawAuxObjects();
    glPopMatrix();

    glFlush ();
}

void Engine3D::DrawAscending3D()
{
	 //this moves the camera to the next point
                VECTOR4D nextStep;
                nextStep = this->getBezierValue(this->pathOrigin,this->pathControl1, this->pathDestination, this->pathControl4,(float)this->pathCounter/this->pathLength);

                VECTOR4D deltaView = Utils3D::glTransformPoint(nextStep,rot_matrix);

                glPushMatrix();
                     glTranslatef(-deltaView.x, -deltaView.y,-deltaView.z);
                     glMultMatrixd(rot_matrix);
                     glGetDoublev(GL_MODELVIEW_MATRIX, rot_matrix);
                glPopMatrix();
                this->updateCameraPosition();


                //the whole idea of the following code is to ensure that when the camera gets to the end of the path
                //the camera is looking down  at the map from the top and the map is aligned across the screen

                //I do this by making sure the camera is always looking in the direction
                //of where pathLookup is at and at the same time ensuring the x axis stays always in the worlds
                //xz axis

                //the world's space y axis from the point of view of the camera
                VECTOR4D worldy(rot_matrix[4], rot_matrix[5], rot_matrix[6],1.0);
                //get the direction in which we should be looking in and project it on the world's xz plane
                VECTOR4D lookupView = Utils3D::glTransformPoint(this->pathLookup,rot_matrix);
                VECTOR4D projectedLookup = lookupView.getProjectionOnPlane(worldy);
                VECTOR4D viewx(1.0,0.0,0.0,1.0);
                VECTOR4D frontView(0.0,0.0,-1.0,1.0);

                //if look up has no component in xz plane
                //this is really a dirty trick to make it do what I want it to
                //but it only applies to this particular case
                if (projectedLookup.magnitude() < Utils3D::TOLERANCE)
                {
                   projectedLookup = (Utils3D::glTransformPoint(this->pathDestination, rot_matrix) - Utils3D::glTransformPoint(this->pathControl4, rot_matrix)).getUnitary();
                }


                //we use this vector as a guide to calculate the angle of rotation around worldy
                VECTOR4D projectedLookupX = (projectedLookup*worldy).getUnitary();


                float cosineOfAngle;
                float angle;

                //this section rotates the camera around the worlds y axis so it aligns

               float progress = (float)this->pathCounter/this->pathLength*0.7 + 0.3;


                VECTOR4D rotationAxis =  projectedLookupX*viewx;

                if (rotationAxis.magnitude() > Utils3D::TOLERANCE)
                {
                    cosineOfAngle = projectedLookupX.dot(viewx);
                    angle = acos(cosineOfAngle)*180/PI;

                    //cout<<"t: "<<this->pathCounter<<" pos"<<this->cameraPosition.toString().toStdString() <<" lookup"<<projectedLookup.toString().toStdString()<<"lookUp magnitude: "<<projectedLookup.magnitude()<<" angle: "<<angle<<" cos: "<<cosineOfAngle<<endl;

                    //rotate around world y to align with the lookup vector
                    glPushMatrix();
                         //rotate around the world's space y axis
                         glRotatef(angle*progress,rotationAxis.x, rotationAxis.y, rotationAxis.z);
                         //cout<<"t: "<<this->pathCounter<<" rotate y "<<angle*progress<<endl;
                         glMultMatrixd(rot_matrix);
                         glGetDoublev(GL_MODELVIEW_MATRIX, rot_matrix);
                    glPopMatrix();
                }

                //this section rotates around camera's x axis
                //supposedly lookupView now should be in the yz plane




                lookupView = Utils3D::glTransformPoint(this->pathLookup,rot_matrix);//.getProjectionOnPlane(viewx);
                lookupView.x = 0;


                if (lookupView.magnitude() > Utils3D::TOLERANCE)
                {
                    lookupView.normalize();
                    rotationAxis = lookupView*frontView;
                    if (rotationAxis.magnitude()> Utils3D::TOLERANCE)
                    {
                        //worldy = VECTOR4D(rot_matrix[4], rot_matrix[5], rot_matrix[6],1.0);
                        //VECTOR4D newViewy = (viewx*lookupView).getUnitary();

                        //do not go upside down
                        if (1)//worldy.dot(newViewy)>= 0)
                        {
                            cosineOfAngle = lookupView.dot(frontView);
                            angle = acos(cosineOfAngle)*180/PI;
                            //rotate around x to align with the lookup vector
                            glPushMatrix();
                                 glRotatef(angle*progress,rotationAxis.x, rotationAxis.y, rotationAxis.z);
                                 //cout<<"t: "<<this->pathCounter<<" rotate x "<<angle*progress<<endl;
                                 glMultMatrixd(rot_matrix);
                                 glGetDoublev(GL_MODELVIEW_MATRIX, rot_matrix);
                            glPopMatrix();
                        }
                        else
                        {
                            //cout<<this->pathCounter<<" wont rotate around x: not upside down"<<endl;
                        }
                        //"cos: "<<cosineOfAngle<<" angle "<<angle<<"axis "<<rotationAxis.toString().toStdString()
                        //cout<<lookupView.toString().toStdString()<<endl;
                    }
                    else
                    {
                        //cout<<this->pathCounter<<" wont rotate in x: front and lookup are almost parallel"<<endl;
                    }
                }
                else
                {
                    //cout<<"projected lookup view too small "<<lookupView.magnitude()<<endl;
                }


                this->DrawAscending();
                this->pathCounter++;
                QPainter painter(this);
                painter.end();    
}


void Engine3D::startAscension()
{
    cout<<"starting ascension path...";
    this->saveCameraTransform();
    //this->resetCameraRotations();
    float offset = 300;
    this->pathCounter = 0;
    this->pathOrigin = cameraPosition;
    this->pathControl1 = cameraPosition + VECTOR4D(-offset,0.0,0.0,1.0);
    this->pathLookup = VECTOR4D(0.0,0.0,-600.0,1.0);



    this->pathDestination = this->pathLookup + VECTOR4D(0.0,700.0,0.0,1.0);
    this->pathControl4 = pathDestination + VECTOR4D(-offset,0.0,0.0,1.0);



    cout<<"origin "<<pathOrigin.toString().toStdString()<<endl;
    cout<<"control1 "<<pathControl1.toString().toStdString()<<endl;
    cout<<"control4 "<<pathControl4.toString().toStdString()<<endl;

    cout<<"destination "<<pathDestination.toString().toStdString()<<endl;
    cout<<"lookup "<<pathLookup.toString().toStdString()<<endl;

    this->applicationState = E_ASCENDING;
}


void Engine3D::startDescension()
{
    this->pathCounter = 0;

    this->pathOrigin = cameraPosition;


    this->pathDestination =
    VECTOR4D(
    this->currpos_matrix[12]*this->currpos_matrix[0]+this->currpos_matrix[13]*this->currpos_matrix[1]+this->currpos_matrix[14]*this->currpos_matrix[2],
    this->currpos_matrix[12]*this->currpos_matrix[4]+this->currpos_matrix[13]*this->currpos_matrix[5]+this->currpos_matrix[14]*this->currpos_matrix[6],
    this->currpos_matrix[12]*this->currpos_matrix[8]+this->currpos_matrix[13]*this->currpos_matrix[9]+this->currpos_matrix[14]*this->currpos_matrix[10]
             )*-1;

    this->pathDestination.y = 0.0;

    VECTOR4D dir = (this->pathOrigin - this->pathDestination);

    float distance = dir.magnitude();
    VECTOR4D vectorcrap = (dir*VECTOR4D(0,1,0,1)).getUnitary()*distance;
    cout<<distance<<" dills: "<<vectorcrap.toString().toStdString()<<endl;
    this->pathControl1 = this->pathOrigin - vectorcrap;


    this->pathLookup = this->pathDestination + dir.getUnitary()*(distance/2);
    this->pathLookup.y = this->pathDestination.y;


    this->pathControl4 = this->pathDestination -  (this->pathLookup - this->pathDestination)*0.5;//VECTOR4D(-offset*10,0.0,0.0,1.0);//

    cout<<"origin "<<pathOrigin.toString().toStdString()<<endl;
    cout<<"control1 "<<pathControl1.toString().toStdString()<<endl;
    cout<<"control4 "<<pathControl4.toString().toStdString()<<endl;
    cout<<"destination "<<pathDestination.toString().toStdString()<<endl;
    cout<<"lookup "<<pathLookup.toString().toStdString()<<endl;

    this->applicationState = E_DESCENDING;
}



VECTOR4D Engine3D::getBezierValue(VECTOR4D origin, VECTOR4D control1,VECTOR4D destination, VECTOR4D control2,float t)
{
    float a = (1-t)*(1-t)*(1-t);
    float b = 3*(1-t)*(1-t)*t;
    float c = 3*(1-t)*t*t;
    float d = t*t*t;
    cout.precision(2);
    //cout<<"Next Step: "<<t<<" "<<a<<" "<<b<<" "<<c<<" "<<d<<endl;

    return origin*a + control1*b + control2*c +  destination*d;
}

void Engine3D::destroySceneObjects()
{
    for(int i=0; i< numObjects; i++)
    {
        delete objects[i];
    }
    delete[] objects;
    this->numObjects = 0;

    this->auxObjects.clear();

#ifdef __WIN32__
    for(int i=0; i < this->numTextures; i++)
    {
            textureLoader->FreeTexture(&textures[i]);
    }
#endif

    delete[] textures;
    this->numTextures = 0;

#ifdef __WIN32__
    for(int i=0; i < this->numAuxTextures; i++)
    {
            textureLoader->FreeTexture(&auxTextures[i]);
    }
#endif

    delete[] auxTextures;
    this->numAuxTextures = 0;
    //destroy readers
    if(this->colladaLoader)
        delete this->colladaLoader;
    if(this->colladaAuxObjects)
        delete this->colladaAuxObjects;    

}

void Engine3D::destroyWidgets3D()
{
    for(int i = 0; i < this->widgets3D.size(); i++)
    {        
        delete this->widgets3D[i];
    }    

#ifdef __WIN32__
    for(int i = 0; i < this->videoWidgets3D.size(); i++)
    {
        if(this->videoWidgets3D[i]->isVideoStarted)
        {
            this->videoWidgets3D[i]->stopVideo();
        }

        delete this->videoWidgets3D[i];
    }
#endif

    this->widgets3D.clear();

#ifdef __WIN32__
    this->videoWidgets3D.clear();
#endif

    if(this->iw)
    {
        delete this->iw;
        this->iw = NULL;
    }
    if(this->iw3)
    {
        delete this->iw3;
        this->iw3 = NULL;
    }
}

//make screenshot of the current frame
void Engine3D::makeScreenShot()
{
    this->screenShot = this->grabFrameBuffer();
}

void Engine3D::renderMenu(QPainter* painter)
{
    menuManager->renderMenu(painter);
}

void Engine3D::updateMenu()
{
    menuManager->updateMenu();
}

void Engine3D::mousePressEvent( QMouseEvent *e )
{
    if(this->applicationState == E_SELECTOR)
        this->processSelectorMouse(e);

    this->menuManager->processMouseEvent(e);

    if(this->popup->click(e))
        this->applicationState = E_FADE_IN;

    if(this->applicationState == E_3DVIEW)
    {
        if(this->menuManager->getCurrent() &&
           this->menuManager->getCurrent()->status == E_HIDDEN)
        {
            //process hits only when menu is not displayed
            this->PickSelected(e->x(), e->y());
        }

        if (this->ProcessMouseEvent(e))
        {
           //get selected object index
           //this->updateWidgetTexture();
           //this->widget->render(this->pbuffer);
        }
        else
        {
            MouseRotX = e->x();
            MouseRotY = e->y();
            MouseActive = true;
        }
    }
}

void Engine3D::mouseReleaseEvent( QMouseEvent *e )
{
   if(this->applicationState == E_SELECTOR)
        this->processSelectorMouse(e);

   MouseActive = false;

   if(this->applicationState == E_3DVIEW)
   {
       if (this->ProcessMouseEvent(e))
       {
           
       }
   }
}

//zoom in / zoom out
void Engine3D::wheelEvent(QWheelEvent *e)
{
    //if(e->state() | Qt::ControlModifier )
    //{
        if(e->delta() > 0)
        {
            zoomSideBar(-0.2);
        }
        else
        {
            zoomSideBar(0.2);
        }
    //}

}

void Engine3D::mouseDoubleClickEvent(QMouseEvent *e)
{
    if(this->applicationState == E_SELECTOR &&
       this->selector == E_SR)
    {
        this->resultManager->processMouseClick(e->x(), e->y());
    }
    else if(this->applicationState == E_3DVIEW)
    {
        this->ProcessMouseEvent(e);
    }
}

void Engine3D::mouseMoveEvent(QMouseEvent *e)
{
   this->menuManager->processMouseEvent(e);

   if(this->applicationState == E_SELECTOR)
        this->processSelectorMouse(e);

   if(this->applicationState == E_3DVIEW)
   {
        if (MouseActive)
        {
            int dX, dY;
            dX = e->x() - MouseRotX;
            dY = MouseRotY - e->y();
            updateCameraRotation(dX,dY);

            MouseRotX = e->x();
            MouseRotY = e->y();
        }
        else
        {
           if (e->buttons() & Qt::LeftButton)//only  when dragging
           {
               if (this->ProcessMouseEvent(e))
               {

                    
               }
           }
        }
    }
}

void Engine3D::populateSearchFilters()
{
    this->searchFilters.clear();

    for(int i = 0; i < this->filterSelector->filterBox->items.size(); i++)
    {
        if(this->filterSelector->filterBox->items[i]->label
           == "Destination name")
        {
            if(this->nameSelector->editBox->items.size())
            {
                NameFilter *nf = new NameFilter("Destination name");
                nf->nameToFilter =
                      this->nameSelector->editBox->items[0]->currText;

                this->searchFilters.append(nf);
            }
        }
        else if(this->filterSelector->filterBox->items[i]->label == "Fees")
        {
            this->searchFilters.append(new PricesFilter("Fees"));
        }
        else if(this->filterSelector->filterBox->items[i]->label == "Services")
        {
            OptionsFilter *of = new OptionsFilter("Services");

            for(int j = 0; j < this->optionsSelector->checkBox->items.size(); j++)
            {
                of->services.append(this->optionsSelector->checkBox->items[i]->label);
            }

            this->searchFilters.append(of);
        }
        else if(this->filterSelector->filterBox->items[i]->label == "Distance")
        {
            DistanceFilter *df = new DistanceFilter("Distance");

            cout << "old RADIUS: " << this->distanceSelector->radius << endl;

            VECTOR4D centre =
                this->getWorldPoint(this->distanceSelector->circleCentreX,
                                this->distanceSelector->circleCentreY);

            VECTOR4D edge =
                this->getWorldPoint(this->distanceSelector->circleEdgeX,
                                this->distanceSelector->circleEdgeY);

            df->centreX = (int)centre.x;
            df->centreZ = (int)centre.z;
            df->edgeX = (int)edge.x;
            df->edgeZ = (int)edge.z;

            df->setRadius();

            cout << "RADIUS: " << df->radius << endl;

            this->searchFilters.append(df);
        }
    }
}

VECTOR4D Engine3D::getWorldPoint(int x, int y)
{    
    VECTOR4D worldPoint;

    //x = this->width()/2;
    //y = this->height()/2;

    cout << "local x, y: " << x << ", " << y << endl;

    GLdouble xd, yd, zd;    

    glMatrixMode(GL_MODELVIEW);

    glPushMatrix();
        this->setCamera();

        GLdouble modelMatrix[16];
        glGetDoublev(GL_MODELVIEW_MATRIX,modelMatrix);
    glPopMatrix();

        glMatrixMode(GL_PROJECTION);
    glPushMatrix();
        glLoadIdentity();
        glFrustum(this->fleft, this->fright, this->fbottom, this->ftop, this->fnear, this->ffar);

        GLdouble projMatrix[16];
        glGetDoublev(GL_PROJECTION_MATRIX,projMatrix);

        glViewport(0,0,(GLsizei)this->screenw, (GLsizei)this->screenh);
        int viewport[4];
        glGetIntegerv(GL_VIEWPORT,viewport);
   glPopMatrix();

    //GLdouble z = (this->cameraPosition.y - this->fnear)/(this->ffar - this->fnear);
    //GLdouble z = (1/this->fnear - 1/10) / (1/this->fnear - 1/this->ffar);
    //GLdouble z = this->fnear*this->ffar / (this->ffar + 10*(this->ffar - this->fnear));

    GLfloat z;
    glReadPixels( x, viewport[3]-y, 1, 1,
        GL_DEPTH_COMPONENT, GL_FLOAT, &z);
    cout << "Z ratio: " << z << endl;

    int err = gluUnProject(
         x,
         this->height()- y,
         z,
         modelMatrix,
         projMatrix,
         viewport,
         //the next 3 parameters are the pointers to the final object
         //coordinates. Notice that these MUST be double's
         &xd, //-&gt; pointer to your own position (optional)
         &yd, // id
         &zd // id
         );

    if(err = GL_TRUE)
        cout << "GL_TRUE" << endl;
    else
        cout << "GL_FALSE" << endl;

    cout << "World point x, y, z: " << xd << ", " <<
            yd << ", " <<  zd << endl;

    worldPoint.x = xd;
    worldPoint.y = yd;
    worldPoint.z = zd;

    return worldPoint;
}

void Engine3D::keyPressEvent( QKeyEvent *e )
{
    switch(this->applicationState)
    {
        case E_3DVIEW:
            this->view3DkeyboardProcess(e);
            break;
        case E_SELECTOR:
            if(this->selector == E_NAME)
                this->nameSelector->processKeyEvent(e);
            break;
        default:
            break;
    }
	this->commonKeyEvent(e);
}

//this are events that are general to the application
void Engine3D::commonKeyEvent(QKeyEvent *e)
{

    switch ( e->key())
    {
        case Qt::Key_M:
            //cycle through the different polygon render modes
            int m[2];
            glGetIntegerv(GL_POLYGON_MODE,m);
            switch (m[0])
            {
                case GL_POINT:
                  glPolygonMode(GL_FRONT, GL_LINE);
                break;

                case GL_LINE:
                   glPolygonMode(GL_FRONT, GL_FILL);
                break;

                case GL_FILL:
                    glPolygonMode(GL_FRONT, GL_POINT);
                break;

                default:
                break;
            }
            break;
          case Qt::Key_L:
              this->turnLightsOn(!this->lightsOn);
              break;
          default:
              break;

    }
}

void Engine3D::view3DkeyboardProcess( QKeyEvent *e )
{
    if(this->selectedWidget != -1)
   {
       QWidget *widgFocus = this->widgets3D[this->selectedWidget]->widget->focusWidget();

       if(widgFocus)
       {
           QApplication::sendEvent(widgFocus, e);
           //this->updateWidgetTexture();
       }
    }




        if( e->key() == Qt::Key_Up)
        {
            this->moveCameraRelative(0, 0, this->speed);
        }
        else if( e->key() == Qt::Key_Down )
        {
            this->moveCameraRelative(0, 0, -this->speed);
        }
        else if( e->key() == Qt::Key_Left )
        {
            this->moveCameraRelative(this->speed, 0, 0);
        }
        else if( e->key() == Qt::Key_Right )
        {
            this->moveCameraRelative(-this->speed, 0, 0);
        }
        else if( e->key() == Qt::Key_PageUp )
        {
            this->moveCameraRelative(0, -this->speed, 0);
        }
        else if( e->key() == Qt::Key_PageDown )
        {
            this->moveCameraRelative(0, this->speed, 0);
        }

        else if( e->key() == Qt::Key_Z )
        {
                this->zoomSideBar(-0.1);
        }

        else if( e->key() == Qt::Key_X )
        {
                this->zoomSideBar(0.1);
        }

        else if( e->key() == Qt::Key_F )
        {
            this->applicationState = E_FADE_IN;
        }

        else if( e->key() == Qt::Key_S )
        {
            /*this->applicationState = E_SELECTOR;
            this->filterSelector->dialogResult = -1;*/
            this->getWorldPoint(0,0);
        }

        else if( e->key() == Qt::Key_P )
        {
            this->optionsSelector->resize(this->size());
        }

        else if (e->key() == Qt::Key_Space)
        {
            this->menuManager->showHideMenu();

        }
        else if (e->key() == Qt::Key_M)
        {
           if (this->popup->status == E_PHIDDEN)
           {
                this->popup->startGrowth();
           }
           else if (this->popup->status == E_PENABLED)
           {
               this->popup->startShrinkage();
           }
        }
        else if (e->key() == Qt::Key_N)
        {

        }
        else if (e->key() == Qt::Key_K)
        {

        }

}

void Engine3D::timeOut()
{
    this->updateMenu();
    this->popup->update();

    update();
}

void Engine3D::timeOutSlot()
{
        timeOut();
}

void Engine3D::textureTimerSlot()
{
#ifdef __WIN32__
    //update textures of all running videos
    if(this->isVideoPlaying)
        this->updateVideoWidgetTexture();

    //update textures of all non-video widgets
    if(!this->isVideoPlaying)
#endif
        this->updateWidgetTexture();
}

void Engine3D::zoomSideBar(float z)
{
	zoom*=(1+z);
}
Engine3D::~Engine3D()
{
#ifdef __WIN32__
    //delete Shake SK7 connectivity pointers
    QObject::disconnect(this->bt, SIGNAL(dataReceived(AccData)), this, SLOT(printAccData(AccData)));
    QObject::disconnect(this->bt, SIGNAL(disconnected(bool)), this, SLOT(isDisconnected(bool)));
    this->bThr->terminate();
    this->bThr->wait();

    delete this->bThr;
    delete this->bt;
#endif
	for(int i=0; i< numObjects; i++)
	{
		delete objects[i];
	}
	delete[] objects;
	delete[] lights;
	
#ifdef __WIN32__
        for(int i=0; i < this->numTextures; i++)
	{
		textureLoader->FreeTexture(&textures[i]);
	}
#endif
	delete[] textures;
#ifdef __WIN32__
        delete textureLoader;
#endif
	delete colladaLoader;
        delete colladaAuxObjects;
	
	//delete destinations
        for(int i = 0; i < this->destinations.size(); i++)
        {
            delete this->destinations[i];
        }
        this->destinations.clear();

        //delete search filters
        for(int i = 0; i < this->searchFilters.size(); i++)
        {
            delete this->searchFilters[i];
        }
        this->searchFilters.clear();

        delete this->routeGraph;

        delete this->distanceSelector;
        delete this->optionsSelector;
        delete this->pricesSelector;
        delete this->nameSelector;
        delete this->filterSelector;
        delete this->srSelector;

        delete this->resultManager;

        cout << "Engine 3D destructor successful!!!" << endl;
}

void Engine3D::restoreCameraPosition()
{
    for(int i = 0; i < 16; i++)
    {
        this->rot_matrix[i] = this->currpos_matrix[i];
    }
    this->updateCameraPosition();
}


void Engine3D::moveCameraRelative(GLfloat right, GLfloat up, GLfloat forward)
{
    if (up)
    {
        //up direction should always be in the +y of world coords
        VECTOR4D vup(rot_matrix[4]*up,rot_matrix[5]*up,rot_matrix[6]*up,1.0);

        glPushMatrix();
             glTranslatef(vup.x,vup.y, vup.z);
             glMultMatrixd(rot_matrix);
             glGetDoublev(GL_MODELVIEW_MATRIX, rot_matrix);
        glPopMatrix();
        this->updateCameraPosition();
    }

    if (right)
    {
        //right direction should always be in the xz plane of world coords
        //VECTOR4D localRight = VECTOR4D(1.0,0.0,0.0,1.0);
        VECTOR4D vright(right*(rot_matrix[6]*rot_matrix[6] + rot_matrix[5]*rot_matrix[5]),-rot_matrix[5]*rot_matrix[4]*right,-rot_matrix[6]*rot_matrix[4]*right,1.0);

        VECTOR4D vrightWorld(-right/right*this->collisionPadding*(vright.x*rot_matrix[0]+vright.y*rot_matrix[1]+vright.z*rot_matrix[2]),-right/right*this->collisionPadding*(vright.x*rot_matrix[4]+vright.y*rot_matrix[5]+vright.z*rot_matrix[6]),-right/right*this->collisionPadding*(vright.x*rot_matrix[8]+vright.y*rot_matrix[9]+vright.z*rot_matrix[10]));

        VECTOR4D tempPos(this->cameraPosition.x + vrightWorld.x,cameraPosition.y + vrightWorld.y,cameraPosition.z + vrightWorld.z,1.0);
        if (!this->checkCollisions(tempPos))
        {
            glPushMatrix();
                 glTranslatef(vright.x,vright.y,vright.z);
                 glMultMatrixd(rot_matrix);
                 glGetDoublev(GL_MODELVIEW_MATRIX, rot_matrix);
            glPopMatrix();
            this->updateCameraPosition();
        }
    }

    if (forward)
    {
        //forward direction should always be in the xz plane of world coords

        //VECTOR4D vforward(-rot_matrix[4]*rot_matrix[6]*forward,-rot_matrix[5]*rot_matrix[6]*forward,forward*(rot_matrix[5]*rot_matrix[5] + rot_matrix[4]*rot_matrix[4]),1.0);
        VECTOR4D xzplane(rot_matrix[4],rot_matrix[5],rot_matrix[6],1.0);
        VECTOR4D vforward = VECTOR4D(0.0,0.0,-1.0,1.0).getProjectionOnPlane(xzplane)*-forward;

        //multiply inverse of rotation matrix (transpose) extracted from rot_matrix by vforward to get the vector in world coords
        //and invert the resulting vector cause apparently its needed...
        //and of course multiply it by the padding
        VECTOR4D vforwardWorld(-forward/forward*this->collisionPadding*(vforward.x*rot_matrix[0]+vforward.y*rot_matrix[1]+vforward.z*rot_matrix[2]),-forward/forward*this->collisionPadding*(vforward.x*rot_matrix[4]+vforward.y*rot_matrix[5]+vforward.z*rot_matrix[6]),-forward/forward*this->collisionPadding*(vforward.x*rot_matrix[8]+vforward.y*rot_matrix[9]+vforward.z*rot_matrix[10]));

        VECTOR4D tempPos(this->cameraPosition.x + vforwardWorld.x,cameraPosition.y + vforwardWorld.y,cameraPosition.z + vforwardWorld.z,1.0);

        if (!this->checkCollisions(tempPos))
        {
            glPushMatrix();
                 glTranslatef(vforward.x,vforward.y,vforward.z);
                 glMultMatrixd(rot_matrix);
                 glGetDoublev(GL_MODELVIEW_MATRIX, rot_matrix);
            glPopMatrix();
            this->updateCameraPosition();
        }
    }
}

void Engine3D::rotateRelativeCamera( GLfloat pitch, GLfloat yaw, GLfloat roll)
{

        if(yaw)
        {
            glPushMatrix();
                //yaw should be constrained to rotation around worldspace axis y
                glRotated(yaw,rot_matrix[4],rot_matrix[5],rot_matrix[6]);
                glMultMatrixd(rot_matrix);
                glGetDoublev(GL_MODELVIEW_MATRIX, rot_matrix);
            glPopMatrix();
        }

        //I guess we should limit the amount of pitch somehow e.g. (-45,45)
        if (fabs(this->currentPitch + pitch) <= this->maxPitch)
        {
            glPushMatrix();

                glRotated(pitch, 1,0,0);
                glMultMatrixd(rot_matrix);
                glGetDoublev(GL_MODELVIEW_MATRIX, rot_matrix);
            glPopMatrix();

            this->currentPitch+= pitch;
            /*
            VECTOR4D front(0,0,-1);
            VECTOR4D xzplane(rot_matrix[4], rot_matrix[5], rot_matrix[6]);
            float sign = xzplane.dot(front)/fabs(xzplane.dot(front));
            this->currentPitch = front.getProjectionOnPlane(xzplane).dot(front)*180/PI*sign;
            */
        }
        //NOTE: by definition we dont have any roll rotations.
}

bool Engine3D::loadTextures()
{
        numTextures = colladaLoader->textureFileNames.size();
	textures = new glTexture[numTextures];
        //GL_REPLACE

#ifdef __WIN32__
        this->textureLoader->SetTextureFilter(txTrilinear);

	for(int i=0; i< numTextures; i++)
	{

            if (! textureLoader->LoadTextureFromDisk(const_cast<char *>(colladaLoader->textureFileNames[i].c_str()), &textures[i]))
            {
                //some error loading textures
                    cout<<"loading error: " << colladaLoader->textureFileNames[i]<<endl;
                    return false;
            }
        }
#endif

        this->numAuxTextures = this->colladaAuxObjects->textureFileNames.size();
        this->auxTextures = new glTexture[numAuxTextures];

#ifdef __WIN32__
        //load auxTextures
        for(int i = 0; i < this->numAuxTextures; i++)
        {
            if (!textureLoader->LoadTextureFromDisk(
               const_cast<char *>(this->colladaAuxObjects->textureFileNames[i].c_str()),
               &this->auxTextures[i]))
            {
                //some error loading textures
                cout<<"loading error: " << this->colladaAuxObjects->textureFileNames[i]<<endl;
                return false;
            }
        }
	
        if (! textureLoader->LoadTextureFromDisk("../res/textures/sky.jpg", &this->skytexture))
            {
                //some error loading textures
                    cout<<"loading error: sky texture"<<endl;
                    return false;
            }
#endif

	return true;
}

bool Engine3D::load3DObjects()
{
        numObjects = colladaLoader->nodes.size();
	objects = new Object3D *[numObjects];
        cout<<"there are "<<numObjects<<" nodes"<<endl;

        for(int i=0; i< numObjects; i++)
	{
                //objects[i] = new Object3D(textures, &colladaLoader->nodes[i],colladaLoader->geometries);
            objects[i] = new Object3D(textures, &colladaLoader->nodes[i],&colladaLoader->geometries);
	}

        cout << "there are " << this->colladaAuxObjects->nodes.size() << " aux nodes" << endl;

        for(u_int i = 0; i <this->colladaAuxObjects->nodes.size(); i++)
        {
            Object3D auxObject(this->auxTextures, &this->colladaAuxObjects->nodes[i],
                &this->colladaAuxObjects->geometries);
            this->auxObjects.append(auxObject);
        }

        //this->createDestinations();

	return true;
}

int Engine3D::getAuxObjIndexByName(string nodeName)
{
    int index = -1;

    for(int i = 0; i < this->auxObjects.size(); i++)
    {
        if(this->auxObjects[i].nodeStr->id == nodeName)
        {
            index = i;
            break;
        }
    }

    return index;
}

int Engine3D::getObjIndexByName(string nodeName)
{
    int index = -1;

    for(int i = 0; i < this->numObjects; i++)
    {
        if(this->objects[i]->nodeStr->id == nodeName)
        {
            index = i;
            break;
        }
    }

    return index;
}

void Engine3D::startDescensionToDestination(VECTOR4D destinationPos, VECTOR4D nodePos)
{

    this->pathCounter = 0;
    this->pathOrigin = cameraPosition;
    VECTOR4D arrivalDirection = nodePos - destinationPos;
    //project to worlds xz plane
    arrivalDirection.y = 0;
    this->pathDestination =  destinationPos - arrivalDirection*0.25;
    this->pathDestination.y = 0.0;
    this->pathControl4 = this->pathDestination - arrivalDirection;

    VECTOR4D destinationDirection = this->pathDestination - this->pathOrigin;
    //project to worlds xz plane
    destinationDirection.y = 0;
    float distance = destinationDirection.magnitude();

    this->pathLookup = nodePos;

    //if the directions are opposing do some kind of loop
    if (destinationDirection.dot(arrivalDirection) < 0)
    {

        this->pathControl1 = this->pathOrigin + (destinationDirection*VECTOR4D(0,1,0,1)).getUnitary()*distance;
    }
    //just go straight up ahead
    else
    {

        this->pathControl1 = this->pathOrigin + destinationDirection*0.5;
    }

    cout<<"origin "<<pathOrigin.toString().toStdString()<<endl;
    cout<<"control1 "<<pathControl1.toString().toStdString()<<endl;
    cout<<"control4 "<<pathControl4.toString().toStdString()<<endl;
    cout<<"destination "<<pathDestination.toString().toStdString()<<endl;
    cout<<"lookup "<<pathLookup.toString().toStdString()<<endl;

    this->applicationState = E_DESCENDING;
}

void Engine3D::createDestinations()
{
    this->createDestItem("McDonalds", "McDonaldsNode", -33.0, -10.0, -680.0,
                         "food");
    this->createDestItem("Ale Pupi", "stockmann", 13.0, -8.0, -49.0);
    this->createDestItem("Shawarma", "stockmann", 42.0, -8.0, -46.0,
                         "food");
    this->createDestItem("Love Hotel", "building21", 3.0, -9.0, -245.0);
    this->createDestItem("Kotibar", "building0", -25.0, -12.0, -555.0);
    this->createDestItem("Harald", "kirjakauppa", -34.0, -11.0, -840.0);
    this->createDestItem("Eurooppa Bar", "scandic", -77.0, -7.0, -126.0);
    this->createDestItem("Public Corner", "building9", -74.0, -10.0, -580.0);
}

void Engine3D::createDestItem(string name, string nodeName,
                              double x, double y, double z, QString auxObjName)
{
    Destination *destMac = new DestinationBar();
    destMac->name = name;
    destMac->nodeName = nodeName;

    int objIndex = this->getObjIndexByName(destMac->nodeName);

    if(objIndex != -1)
    {
        VECTOR4D pos = this->objects[objIndex]->getTransformPosition();
        destMac->nodeCoords.x = pos.x;
        destMac->nodeCoords.y = pos.y;
        destMac->nodeCoords.z = pos.z;
        destMac->nodeCoords.w = pos.w;
    }

    destMac->coords.x = x;
    destMac->coords.y = y;
    destMac->coords.z = z;
    destMac->coords.w = 1.0;
    destMac->auxObjIndex = -1;

    destMac->highLighted = true;


    destMac->auxObjIndex = this->getAuxObjIndexByName(auxObjName.toStdString());
    
    destMac->hilightIndex = this->getAuxObjIndexByName("star");
    this->destinations.append(destMac);
}

void Engine3D::DrawAuxObjects()
{   
    for(int i = 0; i < this->destinations.size(); i++)
    {
        int auxIndex = -1;
        if((auxIndex = this->destinations[i]->auxObjIndex) != -1)
        {
            glPushMatrix();
                glTranslatef(this->destinations[i]->coords.x,
                             this->destinations[i]->coords.y,
                             this->destinations[i]->coords.z);

                glRotatef(this->skyangle*5,0,1,0);
                this->auxObjects[auxIndex].render();
            glPopMatrix();
        }          
    }

    for(int i = 0; i < this->foundDestinations.size(); i++)
    {
        //draw highlights only for found destinations
        if (this->foundDestinations[i]->highLighted &&
            this->foundDestinations[i]->hilightIndex != -1)
        {
            glPushMatrix();

                glTranslatef(this->foundDestinations[i]->coords.x,
                             this->foundDestinations[i]->coords.y + 200,
                             this->foundDestinations[i]->coords.z);
                glRotatef(this->skyangle*5,0,1,0);


                this->auxObjects[this->foundDestinations[i]->hilightIndex].render();
            glPopMatrix();
        }
    }
}

void Engine3D::DrawRouteGraph()
{
    glPushMatrix();
        this->setCamera();
        this->routeGraph->renderGraph();
    glPopMatrix();
}

bool Engine3D::loadLights()
{
	//load light info from scene
	return true;
}


void Engine3D::turnLightsOn(bool on)
{
    this->lightsOn = on;
    if(on)
    {
        int maxLights;
        glGetIntegerv(GL_MAX_LIGHTS, &maxLights);
        cout<<"there are "<<maxLights<<" lights available"<<endl;
        glEnable(GL_LIGHTING);        
        int CurrNumLights = min(this->numlights, maxLights);
        for (int i = 0 ; i< CurrNumLights; i++)
        {
            glEnable(GL_LIGHT0+i);
        }
        glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_MODULATE);

    }
    else
    {
        glDisable(GL_LIGHTING);
        for (int i = 0 ; i< this->numlights; i++)
        {
            glDisable(GL_LIGHT0+i);
        }

        glTexEnvf(GL_TEXTURE_ENV, GL_TEXTURE_ENV_MODE, GL_REPLACE);
    }
}
void Engine3D::illuminate()
{

    if (this->lightsOn)
    {

        GLfloat spot_direction[] = {0.0,-1.0,0.0};
        GLfloat diffuse[] = { 0.3, 0.3, 1.0, 1.0 };
        GLfloat ambient[] = { 0.06, 0.06, 0.2, 1.0 };
        GLfloat lmodel_ambient[] = {0.0,0.0,0.0,1.0};
        GLfloat no_light[] = {0.0,0.0,0.0,1.0};
        glLightModelfv(GL_LIGHT_MODEL_AMBIENT, lmodel_ambient);

        for (int i = 0 ; i< this->numlights; i++)
        {

            GLfloat light_position[] = { -5.0 - 2*i, 100.0, -(i)*150.0, 1.0 };
            glLightfv(GL_LIGHT0+i, GL_POSITION, light_position);
            glLightf(GL_LIGHT0+i, GL_SPOT_CUTOFF, 15.0);
            glLightf(GL_LIGHT0+i, GL_SPOT_EXPONENT, 64.0);
            glLightfv(GL_LIGHT0+i, GL_SPOT_DIRECTION, spot_direction);

            //attenuation
            glLightf(GL_LIGHT0+i, GL_CONSTANT_ATTENUATION, 0.0);
            glLightf(GL_LIGHT0+i, GL_LINEAR_ATTENUATION, 0.0);
            glLightf(GL_LIGHT0+i, GL_QUADRATIC_ATTENUATION, 0.0001);

            glLightfv(GL_LIGHT0+i, GL_DIFFUSE , diffuse);
            glLightfv(GL_LIGHT0+i, GL_AMBIENT, ambient);
            glLightfv(GL_LIGHT0+i, GL_SPECULAR, no_light);

            glPushMatrix();
                glTranslatef(light_position[0],light_position[1],light_position[2]);
                glMaterialfv(GL_FRONT, GL_AMBIENT_AND_DIFFUSE ,diffuse);
                glMaterialfv(GL_FRONT, GL_EMISSION, diffuse);
                gluSphere(this->sky,0.5,20,20);
            glPopMatrix();

        }
    }

}
		/*
	this method should create all the elements of a specified scene:
	3D meshes (e.g ground, buildings, trees, statues, fountains, etc.), textures, lights, etc.
	For each 3D object a Object3D should be created.
	
	*/
bool Engine3D::loadScene(string scenePath, string auxPath)
{
    colladaLoader = new ColladaReader(const_cast<char*>(scenePath.c_str()));
    colladaAuxObjects = new ColladaReader(const_cast<char *>(auxPath.c_str()));

    colladaLoader->getScene();
    colladaAuxObjects->getScene();

	if (loadTextures())
        {
		if (loadLights())
                {
                    if (load3DObjects())
			{
				return true;
			}
			else
			{
				cout<<"error loading objects"<<endl;
			}
		}
		else
		{
			cout<<"error loading lights"<<endl;
		}
	}
	else
	{
		cout<<"error loading textures"<<endl;
	}
	
	return false;
}

bool Engine3D::checkCollisions(VECTOR4D p)
{

    for(int i = 0; i < numObjects; i++)
    {

        if(this->objects[i]->nodeStr->id != "ground" && this->objects[i]->isInsideBDcube(p))
        {


               if(this->inInsideView)
            {
                if(QString(this->objects[i]->nodeStr->id.c_str()).startsWith("wall"))
                {
                    return true;
                }
            }
            else
            {


                return true;
            }
        }

    }

    return false;
}


void Engine3D::setCamera()
{
    glLoadMatrixd(rot_matrix);
}

void Engine3D::setSearchCamera()
{
    glLoadIdentity();
    if (500.0*(1.0-zoom) > -this->ffar + 800.0)
    {
        glTranslatef(0.0,0.0, 500.0*(1.0-zoom));
    }
    else
    {
        glTranslatef(0.0,0.0, -this->ffar + 800.0);
    }
    glMultMatrixd(rot_matrix);
}

void Engine3D::drawAxes(float l, float w, float col)
{
		glColor3f (1.0, 1.0, 1.0);
		//glutSolidSphere(1.0,20,20);
		//char * s = "(0,0,0)\0";
		//renderBitmapString(0,0.15,0,GLUT_BITMAP_8_BY_13,s);
		float axisLenght, axisWidth;
		axisLenght = l;
		axisWidth = w;
		float c = 1.0/(col+1.0);
		glColor3f (c, 0.0, 0.0);
		glPushMatrix();
			glTranslatef(axisLenght/2.0, 0.0, 0.0);
			glScalef(axisLenght/axisWidth, 1.0,1.0);
                        //glutSolidCube(axisWidth);
		glPopMatrix();
		
		glColor3f (0.0, c, 0.0);
		glPushMatrix();
			glTranslatef(0.0, axisLenght/2.0, 0.0);
			glScalef(1.0, axisLenght/axisWidth,1.0);
                        //glutSolidCube(axisWidth);
		glPopMatrix();
	 
	 glColor3f (0.0, 0.0, c);
	  glPushMatrix();
			glTranslatef(0.0, 0.0, axisLenght/2.0);
			glScalef(1.0, 1.0,axisLenght/axisWidth);
                        //glutSolidCube(axisWidth);
		glPopMatrix();
}


void Engine3D::drawCoordRef(int x, int y , int z, float posx, float posy, float l)
{
   /* glPushMatrix();
            glTranslatef(posx,posy, 0);
            if (x) glRotated(x, 1,0,0);
            if (y) glRotated(y, 0,1,0);
            if (z) glRotated(z, 0,0,1);
            //fixed to camera
            drawAxes(l, l/10, 1.0);

            //fixed to the world
            //glMultMatrixf(inv_transform);
            //drawAxes(l, l/10, 0.0);
    glPopMatrix();*/
}


void Engine3D::setSideBarCamera()
{
    //rotate it so we can see it from the top
    glRotated(90,1,0,0);
    //center the map on the cameras current position
    glTranslatef(-this->cameraPosition.x ,-this->cameraPosition.y,-this->cameraPosition.z);
    
}
void Engine3D::DrawSideBar()
{
    glViewport((GLsizei)screenw*2/3,0,(GLsizei)screenw*1/3, (GLsizei)screenh);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    
    float side = 1200.0;
    float screenratio = (float)((float)(this->width()/3.0)/this->height());
    if ( screenratio < 1 )
    {
        glOrtho(-side*zoom*screenratio,side*zoom*screenratio, -side*zoom,side*zoom, -900, 900);
    }
    else
    {
        glOrtho(-side/2*zoom,side/2*zoom, -side*zoom/(screenratio*2),side*zoom/(screenratio*2), -900, 900);
    }


    glMatrixMode(GL_MODELVIEW);


    glDisable(GL_DEPTH_TEST);
    glPushMatrix();


        this->setSideBarCamera();

        this->illuminate();
        for(int i=0; i< numObjects; i++)
        {
                objects[i]->render();
        }        
    glPopMatrix();

    this->drawFrustrum();
    float red[] = {1.0,0.0,0.0};
    glColor3fv(red);
    glMaterialfv(GL_FRONT, GL_EMISSION, red);
    gluSphere(this->cameraLocator,5,20,20);
    glEnable(GL_DEPTH_TEST);
}

void Engine3D::updateSelector(QPainter *painter)
{
    switch(this->selector)
    {        
        case E_NAME:
            if(this->nameSelector->dialogResult == 0)
            {
                this->filterSelector->appendFilter("Destination name");

                //reset dialogResult
                this->nameSelector->resetDialogState();
                this->menuManager->showHideMenu();
                this->selector = E_FILTER;
            }
            else if(this->nameSelector->dialogResult == 1)
            {
                //reset dialogResult
                this->nameSelector->resetDialogState();
                this->menuManager->showHideMenu();
                this->selector = E_FILTER;
            }
            break;
        case E_OPTIONS:
            if(this->optionsSelector->dialogResult == 0)
            {
                this->filterSelector->appendFilter("Services");

                //reset dialogResult
                this->optionsSelector->resetDialogState();
                this->menuManager->showHideMenu();
                this->selector = E_FILTER;
            }
            else if(this->optionsSelector->dialogResult == 1)
            {
                //reset dialogResult
                this->optionsSelector->resetDialogState();
                this->menuManager->showHideMenu();
                this->selector = E_FILTER;
            }
            break;
         case E_PRICES:
            if(this->pricesSelector->dialogResult == 0)
            {
                this->filterSelector->appendFilter("Fees");

                //reset dialogResult
                this->pricesSelector->resetDialogState();
                this->menuManager->showHideMenu();
                this->selector = E_FILTER;
            }
            else if(this->pricesSelector->dialogResult == 1)
            {
                //reset dialogResult
                this->pricesSelector->resetDialogState();
                this->menuManager->showHideMenu();
                this->selector = E_FILTER;
            }
            break;
         case E_DISTANCE:
            if(this->distanceSelector->dialogResult == 0)
            {
                this->filterSelector->appendFilter("Distance");

                //reset dialogResult
                this->distanceSelector->resetDialogState();
                this->menuManager->showHideMenu();
                this->selector = E_FILTER;
            }
            else if(this->distanceSelector->dialogResult == 1)
            {
                //reset dialogResult
                this->distanceSelector->resetDialogState();
                this->menuManager->showHideMenu();
                this->selector = E_FILTER;
            }
            break;
         case E_FILTER:
            //cancel button
            if(this->filterSelector->dialogResult == 1)
            {

                cout<<"cancel button pressed"<<endl;
                this->filterSelector->resetDialogState();
                //restore camera position
                //this->restoreCameraPosition();

                this->menuManager->hideMenuImmediate();
                this->menuManager->setRoot();
                this->startDescension();
            }
            else if(this->filterSelector->dialogResult == 0)
            {
                this->filterSelector->resetDialogState();

                this->menuManager->showHideMenu();

                //populate filters list
                this->populateSearchFilters();                

                //search and fill in results list
                int res = this->destinationsSearch();

                QString resStr;
                resStr.sprintf("Results: %d", res);
                this->srSelector->setResString(resStr);

                //restore camera position
                //this->restoreCameraPosition();
                //this->applicationState = E_3DVIEW;
                this->selector = E_SR;

                //cout << "We are still here!!!" << endl;
            }

            //one of the saved filters
            else if(this->filterSelector->dialogResult == 2)
            {
                if(this->filterSelector->clickedCircleName == "Destination name")
                {
                    this->filterSelector->resetDialogState();
                    this->menuManager->showHideMenu();
                    this->selector = E_NAME;
                }
                else if(this->filterSelector->clickedCircleName == "Fees")
                {
                    this->filterSelector->resetDialogState();
                    this->menuManager->showHideMenu();
                    this->selector = E_PRICES;
                }
                else if(this->filterSelector->clickedCircleName == "Services")
                {
                    this->filterSelector->resetDialogState();
                    this->menuManager->showHideMenu();
                    this->selector = E_OPTIONS;
                }
                else if(this->filterSelector->clickedCircleName == "Distance")
                {
                    this->filterSelector->resetDialogState();
                    this->menuManager->showHideMenu();
                    this->selector = E_DISTANCE;
                }
            }

         case E_SR:
            //go to ramble mode
            if(this->srSelector->dialogResult == 1)
            {
                this->srSelector->resetDialogState();
                //this->restoreCameraPosition();
                //this->selector = E_FILTER;
                //this->applicationState = E_3DVIEW;
                //this->menuManager->showHideMenu();
                this->menuManager->setRoot();
                this->startDescension();
            }
            //go to search (filter selector)
            else if(this->srSelector->dialogResult == 0)
            {
                this->menuManager->showHideMenu();
                this->srSelector->resetDialogState();
                this->selector = E_FILTER;
            }
            break;
         default:
            break;
    }
}

void Engine3D::drawMapMarks(QPainter *painter)
{
    this->resultManager->render(painter);
}

void Engine3D::DrawTopBar()
{
    glViewport(0,(GLsizei)screenh*3/4,(GLsizei)screenw, (GLsizei)screenh/4);
    glMatrixMode(GL_PROJECTION);
    glLoadIdentity();
    glOrtho(0.0,0.6, 0.0, 0.15, -2.0, 2.0);


    glMatrixMode(GL_MODELVIEW);
    drawCoordRef(90,0,0, 0.2, 0.075, 0.05);

    drawCoordRef(0,0,0, 0.35, 0.075, 0.05);

    drawCoordRef(0,-90,0, 0.55, 0.075, 0.05);

    glColor3f(1.0,0.0,0.0);
    printMatrix(10,10, rot_matrix);

    //Utils3D::inverseMatrix4D(rot_matrix,inv_transform);
    //glColor3f(0.0,0.0,1.0);
    //printMatrix(200,10, inv_transform);

    glColor3f(1.0,1.0,1.0);
    QString s;
    s.sprintf("CAMERA: (% 10.2f % 10.2f % 2.2f) PITCH: % 10.2f", this->cameraPosition.x,this->cameraPosition.y ,this->cameraPosition.z, this->currentPitch);
    renderText( 10,100, s);

    glColor3f(0.0,0.0,1.0);
    //QString s;
    s.sprintf("FPS: %4.2f", this->fps);
    renderText( 10,150, s);
}

void Engine3D::printMatrix(int x, int y, Matrix4D m)

{
    QString s;

    for (int i = 0; i<4; i++)
    {
        s.sprintf("% 10.2f % 10.2f % 10.2f% 10.2f", m[0+i],m[4+i] ,m[8+i], m[12+i]);
        renderText( x, y+15*i, s);
    }


}

void Engine3D::updateCameraPosition()
{
    this->cameraPosition.x = -rot_matrix[12]*rot_matrix[0]-rot_matrix[13]*rot_matrix[1]-rot_matrix[14]*rot_matrix[2];
    this->cameraPosition.y = -rot_matrix[12]*rot_matrix[4]-rot_matrix[13]*rot_matrix[5]-rot_matrix[14]*rot_matrix[6];
    this->cameraPosition.z = -rot_matrix[12]*rot_matrix[8]-rot_matrix[13]*rot_matrix[9]-rot_matrix[14]*rot_matrix[10];
}

void Engine3D::DrawMainView()
{
    glMatrixMode(GL_PROJECTION);
    glLoadMatrixd(this->proj_matrix);
    glViewport(0,0,(GLsizei)screenw, (GLsizei)screenh);

    glMatrixMode(GL_MODELVIEW);


    glPushMatrix();
        setCamera();
        //this->updateFrustrumCoords();
        this->drawSky();
        this->illuminate();
        for(int i=0; i< numObjects; i++)
        {
            objects[i]->render();
        }
        this->DrawAuxObjects();
    glPopMatrix();
}


//called by the mouse handler
void Engine3D::updateCameraRotation(int h, int v)
{	
        rotateRelativeCamera(v/20.0,  h/20.0, 0.0);
}

//Menu Manager slot
void Engine3D::menuItemClicked(QString menuItemName)
{
    cout << "Clicked menu item name: " << menuItemName.toStdString() << endl;

    if(menuItemName == "Name")
    {
        this->menuManager->showHideMenu();
        this->selector = E_NAME;
        this->applicationState = E_SELECTOR;
    }
    else if(menuItemName == "Distance")
    {
        this->menuManager->showHideMenu();
        this->selector = E_DISTANCE;
        this->applicationState = E_SELECTOR;
    }
    else if(menuItemName == "Activities")
    {
        this->menuManager->showHideMenu();
        this->selector = E_OPTIONS;
        this->applicationState = E_SELECTOR;
    }
    else if(menuItemName == "Fees")
    {
        this->menuManager->showHideMenu();
        this->selector = E_PRICES;
        this->applicationState = E_SELECTOR;
    }
    else if(menuItemName == "Search")
    {        
        this->startAscension();
        this->selector = E_FILTER;
    }
}

int Engine3D::destinationsSearch()
{
    this->foundDestinations.clear();
    this->resultManager->deleteItems();

    TGlobalFilter globalFilter;

    //apply all search filters to destinations
    for(int i = 0; i < this->searchFilters.size(); i++)
    {
        switch(this->searchFilters[i]->type)
        {
            case E_NAMEFILTER:
            {
                globalFilter.name = ((NameFilter*)this->searchFilters[i])
                                    ->nameToFilter;
                break;
            }
            case E_DISTANCEFILTER:
            {
                globalFilter.centreX = ((DistanceFilter*)this->searchFilters[i])
                                       ->centreX;
                globalFilter.centreZ = ((DistanceFilter*)this->searchFilters[i])
                                       ->centreZ;
                globalFilter.radius = ((DistanceFilter*)this->searchFilters[i])
                                       ->radius;
                break;
            }
            case E_PRICESFILTER:
            {
                break;
            }
            case E_OPTIONSFILTER:
            {
                for(int j = 0; j < ((OptionsFilter*)this->searchFilters[i])
                    ->services.size(); j++)
                {
                    globalFilter.services.append(((OptionsFilter*)
                        this->searchFilters[i])->services[j]);
                }
                break;
            }
            default:
                break;
        }
    }

    for(int i = 0; i < this->destinations.size(); i++)
    {
        bool isAccepted = true;

        //name
        if((globalFilter.name != "*") &&
          (this->destinations[i]->name != globalFilter.name.toStdString()))
        {
            isAccepted = false;
        }

        //distance
        if(globalFilter.radius != 0)
        {
            //if not inside the circle
            if(((this->destinations[i]->coords.x - globalFilter.centreX)
                *(this->destinations[i]->coords.x - globalFilter.centreX) +
               (this->destinations[i]->coords.z - globalFilter.centreZ)
                *(this->destinations[i]->coords.z - globalFilter.centreZ))
                >= globalFilter.radius * globalFilter.radius)
            {
                isAccepted = false;
            }
        }

        if(isAccepted)
        {
            this->foundDestinations.append(this->destinations[i]);
            this->resultManager->addChild(this->destinations[i]->name.c_str());
        }
    }
    cout << "FOUND DESTINATIONS NUMBER: " << this->foundDestinations.size() << endl;
    return this->foundDestinations.size();
}

#ifdef __WIN32__
//SK7 slots implementation
void Engine3D::printAccData(AccData accData)
{
    QTextStream out(stdout);
    /*out << "BT X: " << accData.x << endl;
    out << "BT Y: " << accData.y << endl;
    out << "BT Z: " << accData.z << endl;

    out << "our value: " << accData.y / 980.0 << endl;*/

    if (this->inInsideView)
    {
        accData.x /= 4.0f;
        accData.y /= 4.0f;
        accData.z /= 4.0f;
    }



    GLfloat speed = 2.0;

    if(this->applicationState == E_3DVIEW)
    {
        if(accData.msgType == ACCMSG)
        {
            this->rotateRelativeCamera(0.0, (accData.y / 980.0) * 5, 0.0);

            this->moveCameraRelative(0.0, 0.0, (accData.x / 980.0) * 5);
        }
        else if(accData.msgType == NVDMSG)
        {
            moveCameraRelative(0.0, -speed, 0.0);
        }
        else if(accData.msgType == NVUMSG)
        {
            moveCameraRelative(0.0, speed, 0.0);
        }
        else if(accData.msgType == NVCMSG)
        {
            this->menuManager->showHideMenu();
        }
    }
}
#endif

#ifdef __WIN32__
void Engine3D::isDisconnected(bool disconnected)
{
    static int counter = 0;
    counter++;
    QTextStream out(stdout);

    if(disconnected)
    {
        //out << "counter: " << counter << endl;
        //out << "In Slot 1" << endl;

        while(!this->bThr->isFinished())
        {
        }
        //out << "In Slot 2" << endl;

        this->bThr->start();
    }
}
#endif

void Engine3D::createWidgets3D()
{
    //ImageWidget
    this->iw = new ImageWidget(
            QString("../res/images"));

    //ImageWidget *iw2 = new ImageWidget(
    //        QString("../res/images"));

    //this->iw3 = new ImageWidget(
    //        QString("../res/images"));

    iw->setFixedSize(512, 512);
    //iw2->setFixedSize(512, 512);
   //iw3->setFixedSize(512, 512);

    //"TV-Geometry"
    /*QGroupBox *groupBox = new QGroupBox("Contact Details");
     QLabel *numberLabel = new QLabel("Telephone number");
     QLineEdit *numberEdit = new QLineEdit;

     //this->le = numberEdit;

     QPushButton *but1 =new QPushButton("Button1");
      QPushButton *but2 =new QPushButton("Button2");
       QPushButton *but3 =new QPushButton("Button3");

     QFormLayout *layout = new QFormLayout;

     QDirModel *model = new QDirModel;
     QTreeView *tree = new QTreeView;
     tree->setModel(model);

    QTextEdit* textCrap = new QTextEdit();

     layout->addRow(numberLabel, numberEdit);
     layout->addRow(but1);
     layout->addRow(but2);
     layout->addRow(but3);

     layout->addRow(tree);
     layout->addRow(textCrap);
     groupBox->setLayout(layout);

     groupBox->setFixedSize(256,256);

     //this->treeV = tree;

     //this->widget = groupBox;

     //this->widget->setFocusPolicy(Qt::StrongFocus);

     //this->traverseWidget(this->widget)*/
     this->widgetUnder = NULL;

     QString nodeID("tvPlane");
     Point3D tl, bl, tr, br;
     GLuint texID;

     bool test = false;
#ifdef __WIN32__
     if(test = this->getVerticesAndImageIndex(nodeID, tl, bl, tr, br, texID))
         //this->widgets3D.append(new Widget3D(tl, bl, tr, br, texID, groupBox));
         this->videoWidgets3D.append(new Widget3D(tl, bl, tr, br, texID, NULL, true));
         //this->widgets3D.append(new Widget3D(tl, bl, tr, br, texID, iw2, false));
#endif

     QString nodeID2("galleryPlane_001");

     test = false;
     if(test = this->getVerticesAndImageIndex(nodeID2, tl, bl, tr, br, texID))
         //this->widgets3D.append(new Widget3D(tl, bl, tr, br, texID, groupBox));
         //this->widgets3D.append(new Widget3D(tl, bl, tr, br, texID, groupBox, true));
         this->widgets3D.append(new Widget3D(tl, bl, tr, br, texID, iw, false));


    /*QString nodeID3("WebObj");

     test = false;
     if(test = this->getVerticesAndImageIndex(nodeID3, tl, bl, tr, br, texID))
         //this->widgets3D.append(new Widget3D(tl, bl, tr, br, texID, groupBox));
         //this->widgets3D.append(new Widget3D(tl, bl, tr, br, texID, groupBox, true));
         this->widgets3D.append(new Widget3D(tl, bl, tr, br, texID, iw3, false));*/

}

bool Engine3D::getVerticesAndImageIndex(QString nodeID, Point3D &tl, Point3D &bl,
     Point3D &tr, Point3D &br, GLuint &imageIndex)
{
    int nodeIndex = -1;
    for(u_int i = 0; i < this->colladaLoader->nodes.size(); i++)
    {
        //we found it!!!
        if(this->colladaLoader->nodes[i].id == nodeID.toStdString())
        {
            nodeIndex = i;
        }
    }

    if((nodeIndex == -1) ||
       (this->colladaLoader->nodes[nodeIndex].geometryIDs.size() != 1))
    {
        return false;
    }

    map<string, TGeometry>::iterator it;
    it = this->colladaLoader->geometries.find(
            this->colladaLoader->nodes[nodeIndex].geometryIDs[0]);

    if(it != this->colladaLoader->geometries.end())
    {
        TGeometry gT = it->second;

        if((gT.trianglesStrs.size() == 1) && (gT.trianglesStrs[0].vertices.size() == 18) &&
           (gT.trianglesStrs[0].textureIndex != -1))
        {
            imageIndex = gT.trianglesStrs[0].textureIndex;

            Point3D parr[4];
            u_short p = 0;

            for(u_int i = 0; i < 16; i+=3)
            {
                if(p < 4)
                {
                    if(p)
                    {
                        bool isNotThere = true;
                        for(u_int j = 0; j < p; j++)
                        {
                            isNotThere &= (gT.trianglesStrs[0].vertices[i] != parr[j].x ||
                               gT.trianglesStrs[0].vertices[i+1] != parr[j].y ||
                               gT.trianglesStrs[0].vertices[i+2] != parr[j].z);

                        }

                        if(isNotThere)
                        {
                           parr[p].x = gT.trianglesStrs[0].vertices[i];
                           parr[p].y  = gT.trianglesStrs[0].vertices[i+1];
                           parr[p].z = gT.trianglesStrs[0].vertices[i+2];

                           p++;
                        }
                    }
                    else
                    {
                       parr[p].x = gT.trianglesStrs[0].vertices[i];
                       parr[p].y  = gT.trianglesStrs[0].vertices[i+1];
                       parr[p].z = gT.trianglesStrs[0].vertices[i+2];
                       p++;
                    }
                }

            }


            int tli = 0, bli = 0, tri = 0, bri = 0;

            for(u_int k = 0; k < 4; k++)
            {
                if((parr[k].x <= parr[tli].x) &&
                   (parr[k].y >= parr[tli].y))
                {
                    tli = k;
                }
                if((parr[k].x <= parr[bli].x) &&
                   (parr[k].y <= parr[bli].y))
                {
                    bli = k;
                }
                if((parr[k].x >= parr[tri].x) &&
                   (parr[k].y >= parr[tri].y))
                {
                    tri = k;
                }
                if((parr[k].x >= parr[bri].x) &&
                   (parr[k].y <= parr[bri].y))
                {
                    bri = k;
                }                
                //printf("parr[%d]: %.2f %.2f %.2f\n", k, parr[k].x, parr[k].y, parr[k].z);
            }

            bl = parr[bli];
            tl = parr[tli];
            tr = parr[tri];
            br = parr[bri];

            Matrix4D transMatrix;

            VECTOR4D blv;
            blv.x = bl.x;
            blv.y = bl.y;
            blv.z = bl.z;
            blv.w = 1;

            VECTOR4D tlv;
            tlv.x = tl.x;
            tlv.y = tl.y;
            tlv.z = tl.z;
            tlv.w = 1;

            VECTOR4D trv;
            trv.x = tr.x;
            trv.y = tr.y;
            trv.z = tr.z;
            trv.w = 1;

            VECTOR4D brv;
            brv.x = br.x;
            brv.y = br.y;
            brv.z = br.z;
            brv.w = 1;

            this->getNodeTransform(nodeIndex, transMatrix);
            blv = Utils3D::glTransformPoint(blv, transMatrix);
            tlv = Utils3D::glTransformPoint(tlv, transMatrix);
            trv = Utils3D::glTransformPoint(trv, transMatrix);
            brv = Utils3D::glTransformPoint(brv, transMatrix);

            bl.x = blv.x;
            bl.y = blv.y;
            bl.z = blv.z;

            tl.x = tlv.x;
            tl.y = tlv.y;
            tl.z = tlv.z;

            br.x = brv.x;
            br.y = brv.y;
            br.z = brv.z;

            tr.x = trv.x;
            tr.y = trv.y;
            tr.z = trv.z;

            return true;
        }
    }

    return false;
}

void Engine3D::getNodeTransform(int nodeIndex, Matrix4D transform)
{
    glMatrixMode(GL_MODELVIEW);
    glPushMatrix();
        glLoadIdentity();

        TNode *nodeStr = &this->colladaLoader->nodes[nodeIndex];

        for (u_int i = 0 ; i < nodeStr->translations.size(); i++)
        {            
            glTranslatef(nodeStr->translations[i].translation[0],nodeStr->translations[i].translation[1],nodeStr->translations[i].translation[2]);
        }
        for (u_int i = 0 ; i < nodeStr->rotations.size(); i++)
        {
            glRotatef(nodeStr->rotations[i].rotation[3],nodeStr->rotations[i].rotation[0],nodeStr->rotations[i].rotation[1],nodeStr->rotations[i].rotation[2]);
        }
        for (u_int i = 0 ; i < nodeStr->scalings.size(); i++)
        {
           glScalef(nodeStr->scalings[i].scaling[0],nodeStr->scalings[i].scaling[1],nodeStr->scalings[i].scaling[2]);
        }
        glGetDoublev(GL_MODELVIEW_MATRIX, transform);

        //matrix
        //Utils3D::printMatrix(transform);

     glPopMatrix();
}


bool Engine3D::ProcessMouseEvent(QMouseEvent * e)
{

    //this should only be called when there are 3d widgets in the scene.
      this->updateRay(e->x(), e->y());


      for(int i = 0; i < this->widgets3D.size(); i++)
      {
         Point3D nearr(this->nearPosX, this->nearPosY, this->nearPosZ);
         Point3D farr(this->farPosX, this->farPosY, this->farPosZ);

         Widget3D *widget3D = this->widgets3D.at(i);

         bool isIntersecting = widget3D->intersects(nearr, farr);

         if(isIntersecting
#ifdef __WIN32__
            && !widget3D->isVideoWidget
#endif
          )
         {
            //set selected widget and stop video
            this->selectedWidget = i;
#ifdef __WIN32__
            cout << "SELECTED VIDEO WIDGET: " << this->selectedVideoWidget << endl;
            if(this->selectedVideoWidget != -1)
            {
                cout << "SELECTED VIDEO WIDGET: " << this->selectedVideoWidget << endl;
                this->videoWidgets3D[this->selectedVideoWidget]->stopVideo();
            }
            this->selectedVideoWidget = -1;
            this->isVideoPlaying = false;
#endif
            Point3D interPoint = widget3D->getLocalCoord();

            //cout << "Texture name: " << this->colladaLoader->textureFileNames[widget3D->imageIndex] << endl;

            QPoint clickPoint((int)(interPoint.x * widget3D->widget->width()), (int)(interPoint.y * widget3D->widget->height()));

            QWidget *widgAt = widget3D->widget->childAt(clickPoint);
            if (! widgAt)
            {
                widgAt = widget3D->widget;
            }
            QWidget * destinW = NULL;
            if(widgAt)
            {
                if (e->type() == QEvent::MouseButtonPress)
                {
                    destinW = widgAt;
                    this->widgetUnder = widgAt;
                    widgAt->setFocus(Qt::MouseFocusReason);
                    printf("mousepress");
                }

                else if (e->type() == QEvent::MouseButtonRelease)
                {
                    /*if (this->widgetUnder && widgAt != this->widgetUnder)
                    {

                        QMouseEvent evt(e->type(), widgAt->mapFrom(this->widget,clickPoint), e->button(), e->buttons(), e->modifiers());
                        QApplication::sendEvent(widgAt, &evt);
                        destinW = this->widgetUnder;
                    }
                    else
                    {*/
                        destinW = widgAt;
                    //}
                    this->widgetUnder = NULL;
                }
                else if (e->type() == QEvent::MouseMove)
                {
                   if (this->widgetUnder && widgAt != this->widgetUnder)
                    {
                        //
                        QMouseEvent releaseevt(QEvent::MouseButtonRelease, this->widgetUnder->mapFrom(widget3D->widget,clickPoint), Qt::LeftButton,Qt::NoButton , e->modifiers());
                        QApplication::sendEvent(this->widgetUnder, &releaseevt);

                        //
                        QMouseEvent pressevt(QEvent::MouseButtonPress, widgAt->mapFrom(widget3D->widget,clickPoint), Qt::LeftButton,Qt::NoButton , e->modifiers());
                        QApplication::sendEvent(widgAt, &pressevt);

                        this->widgetUnder = widgAt;
                        printf("mouse move diff");
                    }
                   destinW = widgAt;
                }
                else //other events
                {
                    destinW = widgAt;
                }

                QMouseEvent evt(e->type(), destinW->mapFrom(widget3D->widget,clickPoint), e->button(), e->buttons(), e->modifiers());
                QApplication::sendEvent(destinW, &evt);


            }
            else
            {
               QMouseEvent evt(e->type(), clickPoint, e->button(), e->buttons(), e->modifiers());
               QApplication::sendEvent(widget3D->widget, &evt);
            }

            return true;
         }
       }

      if(e->type() == QEvent::MouseButtonPress)
      {
#ifdef __WIN32__
          for(int i = 0; i < this->videoWidgets3D.size(); i++)
          {
             Point3D nearr(this->nearPosX, this->nearPosY, this->nearPosZ);
             Point3D farr(this->farPosX, this->farPosY, this->farPosZ);

             Widget3D *widget3D = this->videoWidgets3D.at(i);

             bool isIntersecting = widget3D->intersects(nearr, farr);

              //video widget
             if(isIntersecting && widget3D->isVideoWidget)
             {
                 this->selectedVideoWidget = i;

                 //cout << "Selected VIDEO WIDGET: " << this->selectedVideoWidget << endl;

                 this->selectedWidget = -1;
                 this->isVideoPlaying = true;

                 if(!widget3D->isVideoStarted)
                    widget3D->startVideo();
                 else
                 {
                    widget3D->stopVideo();
                    this->isVideoPlaying = false;
                 }

                 return true;
             }
          }
#endif
      }
      this->selectedWidget = -1;

      return false;
}

void Engine3D::PickSelected(int x, int y)
{

    bool lightsWereOn = this->lightsOn;
    this->turnLightsOn(false);
    int x_vp = x;
    int y_vp = y;

    glViewport(0,0,(GLsizei)screenw, (GLsizei)screenh);
    GLint viewport[4];
    glGetIntegerv(GL_VIEWPORT,viewport);



    glSelectBuffer(SELBUFSIZE, this->selectBuf);
    glRenderMode(GL_SELECT);

    glMatrixMode(GL_PROJECTION);

    glPushMatrix();
    glLoadIdentity();


    gluPickMatrix(x_vp, screenh - y_vp,5, 5, viewport);
    glFrustum(this->fleft, this->fright, this->fbottom, this->ftop, this->fnear, this->ffar);

    glMatrixMode(GL_MODELVIEW);
    glInitNames();

      glPushMatrix();
                setCamera();
                for(int i = 0; i < numObjects; i++)
                {
                      glPushName(i);
                      objects[i]->render();
                      glPopName();
                }


                //aux objects
                //only if we are not in the inside view
                if(!this->inInsideView)
                {
                    for(int j = 0; j < this->destinations.size(); j++)
                    {
                        int auxIndex = -1;
                        if((auxIndex = this->destinations[j]->auxObjIndex) != -1)
                        {
                            glPushMatrix();
                                glTranslatef(this->destinations[j]->coords.x,
                                             this->destinations[j]->coords.y,
                                             this->destinations[j]->coords.z);
                                glPushName(j + 100000);
                                    this->auxObjects[auxIndex].render();
                                glPopName();
                            glPopMatrix();
                        }                        
                    }

                    for(int j = 0; j < this->foundDestinations.size(); j++)
                    {
                        int hilightIndex = -1;
                        if((hilightIndex =
                            this->foundDestinations[j]->hilightIndex) != -1)
                        {
                            glPushMatrix();
                                glTranslatef(this->foundDestinations[j]->coords.x,
                                             this->foundDestinations[j]->coords.y + 200,
                                             this->foundDestinations[j]->coords.z);
                                glPushName(j + 1000000);
                                    this->auxObjects[hilightIndex].render();
                                glPopName();
                            glPopMatrix();
                        }
                    }
                }

      glPopMatrix();

    int hits;

    // restoring the original projection matrix
    glMatrixMode(GL_PROJECTION);
    glPopMatrix();
    glMatrixMode(GL_MODELVIEW);
    glFlush();

    // returning to normal rendering mode
    hits = glRenderMode(GL_RENDER);

    // if there are hits  them
    if (hits != 0)
    {
      cout << "hits number: " << hits << endl;
      processHits(hits,this->selectBuf);
    }
    this->turnLightsOn(lightsWereOn);
}

void Engine3D::resizeSelector(QSize size)
{   
   if(this->nameSelector &&
      this->filterSelector &&
      this->optionsSelector &&
      this->pricesSelector &&
      this->distanceSelector &&
      this->srSelector)
   {
       this->nameSelector->resize(size);
       this->filterSelector->resize(size);
       this->optionsSelector->resize(size);
       this->pricesSelector->resize(size);
       this->distanceSelector->resize(size);
       this->srSelector->resize(size);
   }
}

void Engine3D::processSelectorMouse(QMouseEvent *e)
{
    //this->distanceSelector->processMouseEvent(e);
    //this->optionsSelector->processMouseEvent(e);
    //this->pricesSelector->processMouseEvent(e);
    //this->nameSelector->processMouseEvent(e);
    //this->filterSelector->processMouseEvent(e);

    switch(this->selector)
    {
        case E_NAME:
            this->nameSelector->processMouseEvent(e);
            break;
        case E_FILTER:
            this->filterSelector->processMouseEvent(e);
            break;
        case E_DISTANCE:
            this->distanceSelector->processMouseEvent(e);
            break;
        case E_OPTIONS:
            this->optionsSelector->processMouseEvent(e);
            break;
        case E_PRICES:
            this->pricesSelector->processMouseEvent(e);
            break;
        case E_SR:
            this->srSelector->processMouseEvent(e);
            break;
        default:
            break;
    }
}

void Engine3D::DrawRectangle(int red, int green, int blue, int alpha, QPainter &painter)
{
      QBrush brush(QColor(red, green, blue, alpha));
      painter.setBrush(brush);
      painter.drawRect(this->rect());
}

void Engine3D::processHits (GLint hits, GLuint buffer[])
{
   GLuint names, *ptr, minZ, *ptrNames, numberOfNames;

   ptr = (GLuint *) buffer;
   minZ = 0xffffffff;

   for (int i = 0; i < hits; i++)
   {
       names = *ptr;
       ptr++;

       if (*ptr < minZ)
       {
            numberOfNames = names;
            minZ = *ptr;
            ptrNames = ptr + 2;
       }

       ptr += names+2;
    }

      /*cout << "The closest hit names are ";
      ptr = ptrNames;
      
      for (u_int j = 0; j < numberOfNames; j++,ptr++)
      {
         cout << *ptr;
      }
      cout << endl;*/

   ptr = ptrNames;
   //hit object name
   if(numberOfNames > 0)
   {
       if(*ptr < 100000)
       {
           cout << "Hit object node name: " <<
                    this->objects[*ptr]->nodeStr->id << endl;

           this->objects[*ptr]->risen = !this->objects[*ptr]->risen;
           this->objects[*ptr]->showBoundingBox(!this->objects[*ptr]->isBoundingBoxVisible());


           //exit from the door of the inside view
           if(this->inInsideView && this->objects[*ptr]->nodeStr->id == "door")
           {
               this->applicationState = E_FADE_IN;
#ifdef __WIN32__
               //close all videos if any
               for(int k = 0; k < this->videoWidgets3D.size(); k++)
               {
                   if(this->videoWidgets3D[k]->isVideoStarted)
                        this->videoWidgets3D[k]->stopVideo();
               }
               this->isVideoPlaying = false;
#endif
           }
           //show price list for drinks
           else if(this->inInsideView && this->objects[*ptr]->nodeStr->id == "bottlesPlane")
           {
                QString currInfo = "Food and Drinks";
                this->currObjInfo.tailCoords =
                        this->objects[*ptr]->getTransformPosition();

                if (this->popup->status == E_PHIDDEN)
                {
                     this->currObjInfo.info = currInfo;
                     this->popup->startGrowth();
                }
                else if (this->popup->status == E_PENABLED &&
                         this->currObjInfo.info == currInfo)
                {
                    this->popup->startShrinkage();
                }
                else if (this->popup->status == E_PENABLED &&
                         this->currObjInfo.info != currInfo)
                {
                    this->currObjInfo.info = currInfo;
                    this->popup->startGrowth();
                }
           }
           //show karaoke list
           else if(this->inInsideView && this->objects[*ptr]->nodeStr->id == "karaoke")
           {
                QString currInfo = "Karaoke song list";
                this->currObjInfo.tailCoords =
                        this->objects[*ptr]->getTransformPosition();

                if (this->popup->status == E_PHIDDEN)
                {
                     this->currObjInfo.info = currInfo;
                     this->popup->startGrowth();
                }
                else if (this->popup->status == E_PENABLED &&
                         this->currObjInfo.info == currInfo)
                {
                    this->popup->startShrinkage();
                }
                else if (this->popup->status == E_PENABLED &&
                         this->currObjInfo.info != currInfo)
                {
                    this->currObjInfo.info = currInfo;
                    this->popup->startGrowth();
                }
           }
           //get directions
           else if(this->inInsideView && this->objects[*ptr]->nodeStr->id == "compassPlane")
           {
                QString currInfo = "Get Directions";
                this->currObjInfo.tailCoords =
                        this->objects[*ptr]->getTransformPosition();

                if (this->popup->status == E_PHIDDEN)
                {
                     this->currObjInfo.info = currInfo;
                     this->popup->startGrowth();
                }
                else if (this->popup->status == E_PENABLED &&
                         this->currObjInfo.info == currInfo)
                {
                    this->popup->startShrinkage();
                }
                else if (this->popup->status == E_PENABLED &&
                         this->currObjInfo.info != currInfo)
                {
                    this->currObjInfo.info = currInfo;
                    this->popup->startGrowth();
                }
           }
       }
       else if(*ptr >= 100000 && *ptr < 1000000)
       {
            if((*ptr - 100000) < this->destinations.size())
            {
                cout << "Destination name: " <<
                        this->destinations[*ptr - 100000]->name << endl;

                if (this->popup->status == E_PHIDDEN)
                {
                     this->currDestination = this->destinations[*ptr - 100000];
                     this->popup->startGrowth();
                }
                else if (this->popup->status == E_PENABLED &&
                         this->currDestination == this->destinations[*ptr - 100000])
                {
                    this->popup->startShrinkage();
                }
                else if (this->popup->status == E_PENABLED &&
                         this->currDestination != this->destinations[*ptr - 100000])
                {
                    this->currDestination = this->destinations[*ptr - 100000];
                    this->popup->startGrowth();
                }
            }

            /*if(this->auxObjects[*ptr - 100000].nodeStr->id == "Can")
            {
                this->applicationState = E_FADE_IN;
                //this->menuManager->showHideMenu();
            }*/
       }
       else if(*ptr >= 1000000)
       {
           if((*ptr - 1000000) < this->foundDestinations.size())
           {
                cout << "hilight found object destination was hit: " <<
                        this->foundDestinations[*ptr - 1000000]->name << endl;
                this->jumpToDestination(
                        this->foundDestinations[*ptr - 1000000]->coords,
                        this->foundDestinations[*ptr - 1000000]->nodeCoords);
           }
       }
   }
}

void Engine3D::jumpToDestination(VECTOR4D destinationPos, VECTOR4D nodePos)
{

    this->pathCounter = 0;
    this->pathOrigin = cameraPosition;
    VECTOR4D arrivalDirection = nodePos - destinationPos;
    //project to worlds xz plane
    arrivalDirection.y = 0;
    this->pathDestination =  destinationPos - arrivalDirection*0.25;
    this->pathDestination.y = 0.0;
    this->pathControl4 = this->pathDestination - arrivalDirection;

    VECTOR4D destinationDirection = this->pathDestination - this->pathOrigin;
    //project to worlds xz plane
    destinationDirection.y = 0;
    float distance = destinationDirection.magnitude();

    this->pathLookup = nodePos;

    //if the directions are opposing do some kind of loop
    if (destinationDirection.dot(arrivalDirection) < 0)
    {

        this->pathControl1 = this->pathOrigin + (destinationDirection*VECTOR4D(0,1,0,1)).getUnitary()*distance + VECTOR4D(0.0,400.0,0.0,1.0);
    }
    //just go straight up ahead
    else
    {

        this->pathControl1 = this->pathOrigin + VECTOR4D(0.0,400.0,0.0,1.0);
    }

    cout<<"origin "<<pathOrigin.toString().toStdString()<<endl;
    cout<<"control1 "<<pathControl1.toString().toStdString()<<endl;
    cout<<"control4 "<<pathControl4.toString().toStdString()<<endl;
    cout<<"destination "<<pathDestination.toString().toStdString()<<endl;
    cout<<"lookup "<<pathLookup.toString().toStdString()<<endl;

    this->applicationState = E_DESCENDING;
}

QPointF Engine3D::getScreenCoords(VECTOR4D worldCoords)
{
    //project the point to the screen
        GLdouble xd, yd, zd;

        int viewport[4];
        viewport[0] = 0;
        viewport[1] = 0;
        viewport[2] = this->screenw;
        viewport[3] = this->screenh;

       gluProject(
                worldCoords.x,
                worldCoords.y,
                worldCoords.z,
                this->rot_matrix,
                this->proj_matrix,
                viewport,
                &xd,
                &yd,
                &zd);

       if(zd > 0.0 && zd < 1.0 && yd > 0.0 && yd < this->screenh &&
          xd > 0.0 && xd < this->screenw)
       {
           return QPointF(xd, this->screenh - yd);
       }
       else
       {
           return QPointF();
       }
}

void Engine3D::updateFrustrumCoords()
{
        GLdouble modelMatrix[16];
        glGetDoublev(GL_MODELVIEW_MATRIX,modelMatrix);

        GLdouble projMatrix[16];
        glGetDoublev(GL_PROJECTION_MATRIX,projMatrix);

        int viewport[4];
        glGetIntegerv(GL_VIEWPORT,viewport);



        for (int i= 0; i<2;  i++)
            for (int j= 0; j<2;  j++)
            {


            gluUnProject(
                 i*this->screenw,
                 j*this->screenh,
                 0.0,
                 modelMatrix,
                 projMatrix,
                 viewport,
                 //the next 3 parameters are the pointers to the final object
                 //coordinates. Notice that these MUST be double's
                 &frustrumCoords[(i*2 + j)*6], //-&gt; pointer to your own position (optional)
                 &frustrumCoords[(i*2 + j)*6 +1], // id
                 &frustrumCoords[(i*2 + j)*6 +2] // id
                 );

            gluUnProject(
                 i*this->screenw,
                 j*this->screenh,
                 1.0,
                 modelMatrix,
                 projMatrix,
                 viewport,
                 //the next 3 parameters are the pointers to the final object
                 //coordinates. Notice that these MUST be double's
                 &frustrumCoords[(i*2 + j)*6 + 3], //-&gt; pointer to your own position (optional)
                 &frustrumCoords[(i*2 + j)*6 + 4], // id
                 &frustrumCoords[(i*2 + j)*6 + 5] // id
                 );
               }
}

void Engine3D::drawFrustrum()
{
    glColor3d(0,0,1);
        glBegin(GL_LINE_LOOP);
            glVertex3f(frustrumCoords[0], frustrumCoords[1], frustrumCoords[2]);
            glVertex3f(frustrumCoords[3], frustrumCoords[4], frustrumCoords[5]);
            glVertex3f(frustrumCoords[9], frustrumCoords[10], frustrumCoords[11]);
            glVertex3f(frustrumCoords[6], frustrumCoords[7], frustrumCoords[8]);
        glEnd();

        glBegin(GL_LINE_LOOP);
            glVertex3f(frustrumCoords[12], frustrumCoords[13], frustrumCoords[14]);
            glVertex3f(frustrumCoords[15], frustrumCoords[16], frustrumCoords[17]);
            glVertex3f(frustrumCoords[21], frustrumCoords[22], frustrumCoords[23]);
            glVertex3f(frustrumCoords[18], frustrumCoords[19], frustrumCoords[20]);
        glEnd();

        glBegin(GL_LINE_LOOP);
            glVertex3f(frustrumCoords[0], frustrumCoords[1], frustrumCoords[2]);
            glVertex3f(frustrumCoords[6], frustrumCoords[7], frustrumCoords[8]);
            glVertex3f(frustrumCoords[18], frustrumCoords[19], frustrumCoords[20]);
            glVertex3f(frustrumCoords[12], frustrumCoords[13], frustrumCoords[14]);
        glEnd();

        glBegin(GL_LINE_LOOP);
            glVertex3f(frustrumCoords[3], frustrumCoords[4], frustrumCoords[5]);
            glVertex3f(frustrumCoords[9], frustrumCoords[10], frustrumCoords[11]);
            glVertex3f(frustrumCoords[21], frustrumCoords[22], frustrumCoords[23]);
            glVertex3f(frustrumCoords[15], frustrumCoords[16], frustrumCoords[17]);
        glEnd();


}
void Engine3D::updateRay(int x, int y)
{

        int viewport[4];
        viewport[0] = 0;
        viewport[1] = 0;
        viewport[2] = this->screenw;
        viewport[3] = this->screenh;

        gluUnProject(
             x,
             this->height()- y,
             0.0,
             this->rot_matrix,
             this->proj_matrix,
             viewport,
             //the next 3 parameters are the pointers to the final object
             //coordinates. Notice that these MUST be double's
             &this->nearPosX, //-&gt; pointer to your own position (optional)
             &this->nearPosY, // id
             &this->nearPosZ // id
             );

        gluUnProject(
             x,
             this->height()-y,
             1.0,
             this->rot_matrix,
             this->proj_matrix,
             viewport,
             //the next 3 parameters are the pointers to the final object
             //coordinates. Notice that these MUST be double's
             &this->farPosX, //-&gt; pointer to your own position (optional)
             &this->farPosY, // id
             &this->farPosZ // id
             );
}

#ifdef __WIN32__
void Engine3D::updateVideoWidgetTexture()
{
    //update textures of all video widgets if the video is running
    for(int i = 0; i < this->videoWidgets3D.size(); i++)
    {
        if(this->videoWidgets3D[i]->isVideoWidget &&
           this->videoWidgets3D[i]->isVideoStarted &&
           this->videoWidgets3D[i]->ffMpegReader &&
           this->videoWidgets3D[i]->ffMpegReader->isOpened)
        {
            if(!this->videoWidgets3D[i]->ffMpegReader->textures.isEmpty())
            {
                QImage tmp = this->videoWidgets3D[i]->
                             ffMpegReader->textures.dequeue().scaled(QSize(256,256));

                QImage texImg = QGLWidget::convertToGLFormat(tmp);

                glTexture *textureStruct = &this->textures[this->videoWidgets3D[i]->imageIndex];

                glDeleteTextures(1, &textureStruct->TextureID);

                glGenTextures(1, &textureStruct->TextureID);

                glBindTexture(GL_TEXTURE_2D, textureStruct->TextureID);

                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
                glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

                glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
                glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);

                glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, texImg.width(), texImg.height(),
                    0, GL_RGBA, GL_UNSIGNED_BYTE, texImg.bits());
            }
            else
            {
                //cout << "TEXTURE QUEUE is EMPTY!!!" << endl;
            }
        }
    }
}
#endif

void Engine3D::updateWidgetTexture()
{
    for(int i = 0; i < this->widgets3D.size(); i++)
    {
#ifdef __WIN32__
        if(!this->widgets3D[i]->isVideoWidget)
#endif
        {

            QPixmap widgetPM(this->widgets3D[i]->widget->size());
            this->widgets3D[i]->widget->render(&widgetPM);
            QImage widgText = widgetPM.toImage();

            QImage imm = QGLWidget::convertToGLFormat(widgText);

            glTexture *textureStruct = &this->textures[this->widgets3D[i]->imageIndex];

            //cout << "Texture ID before: " << textureStruct->TextureID << endl;
            //cout << "Texture ID before: " << this->textures[this->widgets3D[this->selectedWidget]->imageIndex].TextureID << endl;

            glDeleteTextures(1, &textureStruct->TextureID);

            glGenTextures(1, &textureStruct->TextureID);

            //cout << "Texture ID after: " << textureStruct->TextureID << endl;

            glBindTexture(GL_TEXTURE_2D, textureStruct->TextureID);

            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_S, GL_REPEAT);
            glTexParameteri(GL_TEXTURE_2D, GL_TEXTURE_WRAP_T, GL_REPEAT);

            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MAG_FILTER,GL_LINEAR);
            glTexParameteri(GL_TEXTURE_2D,GL_TEXTURE_MIN_FILTER,GL_LINEAR);


            glTexImage2D(GL_TEXTURE_2D, 0, GL_RGBA, imm.width(), imm.height(),
                0, GL_RGBA, GL_UNSIGNED_BYTE, imm.bits());
        }
    }

}
