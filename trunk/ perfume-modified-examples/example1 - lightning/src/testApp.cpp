#include "testApp.h"

class Tracker;
class Particle;
class ParticleSystem;

const float trackDuration = 64.28;
ofVec3f center, center_t;
ofVec3f campos, campos_t;
ofVec3f offset, offset_v;
vector<Tracker*> trackers;

/* classes for handling particles */

//--------------------------------------------------------------
// class used for tracking information relevant to a particle
class Particle {
private:
	ofVec3f pos;
	ofVec3f heading;
	float lifespan;
	int type;

public:
	void init(ofVec3f pos_, ofVec3f heading_, float lifespan_, int type_) {
		pos = pos_;
		heading = heading_;
		lifespan = lifespan_;
		type = type_;
	}

	// getters and setters
	void setPos(ofVec3f pos_) {
		pos = pos_;
	}
	ofVec3f getPos() {
		return pos;
	}
	ofVec3f getHeading() {
		return heading;
	}
	void setLifespan(float lifespan_) {
		lifespan = lifespan_;
	}
	float getLifespan() {
		return lifespan;
	}
	int getType() {
		return type;
	}
	void setType(int type_) {
		type = type_;
	}
};

// class for handling creation and updating of particles
class ParticleSystem {
private:
	vector<Particle> particleVec;

public:
	// create a particle at a given location with a movement direction and lifespan
	void emit(ofVec3f pos_, ofVec3f heading_, float lifespan_, int type_) {
		Particle next;
		next.init(pos_, heading_, lifespan_, type_);
		particleVec.push_back(next);
	}

	// move particles according to their current direction and update their lifespan
	void updateParticles() {
		for (int i = 0; i < particleVec.size(); i++) {
			particleVec[i].setPos(particleVec[i].getPos() + particleVec[i].getHeading());
			particleVec[i].setLifespan(particleVec[i].getLifespan()-1);
		}
	}

	// remove particles from the system when they run out of lifespan
	void checkLifespans() {
		for (int i = 0; i < particleVec.size(); i++) {
			if (particleVec[i].getLifespan() < 0) {
				particleVec[i].setType(-1);
				if (particleVec[0].getType() == -1) {
					particleVec.erase(particleVec.begin());
				}
			}
		}
	}

	// getters and setters
	size_t getSize() {
		return particleVec.size();
	}
	vector<Particle> getParticles() {
		return particleVec;
	}
};


/* The Tracker class handles the motion of each figure in the scene.  Each Tracker object tracks the position data of one figure in the scene, 
   as well as visual effects related to that figure.  The Tracker also stores the position data of the other two figures, which allows the figure 
   the Tracker handles to interact with the others. */

//--------------------------------------------------------------
class Tracker
{
public:
	
	ofxBvh *bvh, *bvhL, *bvhR;
	vector<ofxBvh> all;
	int id;
	
	// Frames hold the set of position values in each frame of movement
	typedef vector<ofVec3f> Frame;
	// each track contains the position values of all frames for each figure
	typedef deque<Frame> Track;
	Track track, lTrack, rTrack;
	int numPoints;
	float boltTime;
	bool drawBolt;
	bool leftTarget, rightTarget;
	int placeCount, direction, segment, numBolts;
	int modifier[1024];
	int startIndices[64], endIndices[64];
	
	struct Buffer
	{
		ofVec3f v1, v2;
		ofVec3f norm;
	};
	typedef vector<Buffer> BufferArray;
	vector<BufferArray> buffer;
	ParticleSystem particleHandler;
	Frame startPoints, lPoints, rPoints;
	
	// initialize values
	void setup(ofxBvh *o, int id_)
	{
		bvh = o;
		boltTime = 0;
		drawBolt = false;
		placeCount = 0;
		modifier[0] = 0;
		id = id_;
	}
	// set which figures are to the left and right of this figure
	void setBvhL(ofxBvh *o) {
		bvhL = o;
	}
	void setBvhR(ofxBvh *o) {
		bvhR = o;
	}

	// add position values and update other tracker values
	void update()
	{
		if (bvh->isFrameNew())
		{
			// add the position data in the current frame to the tracker
			Frame f, lF, rF;
			addFrame(&f, bvh, &track);
			// also keep track of the position data of the other two figures
			addFrame(&lF, bvhL, &lTrack);
			addFrame(&rF, bvhR, &rTrack);	
			startPoints = track[0];
			lPoints = lTrack[0];
			rPoints = rTrack[0];

			modifyVertices();
			cacheVertices();
			handleParticles();
		}
	}

	// draw the elements in the scene
	void draw()
	{
		glEnable(GL_POLYGON_OFFSET_FILL);
		glPolygonOffset(1, 1);

		drawParticles();
		drawFigure();
		handleBolts();

		int fade = 100-boltTime;
		ofVec3f mid, last;
		int widths[16];
		ofColor colors[16];
		widths[0] = 3+rand()%3;
		widths[1] = 5+rand()%2;
		widths[2] = 10;
		colors[0] = ofColor(255, 255, 255, 110-fade);
		colors[1] = ofColor(100, 100, 225, 20);
		colors[2] = ofColor(0, 20, 225, 100-fade);
		// draw "lightning bolts" using the values assigned above
		for (int n = 0; n < 5; n++)
			renderBolt(last, mid, numPoints, fade, startPoints, startIndices[n], endIndices[n], widths, colors, 30, 2, 1);
		// determine which figures to connect larger bolts to
		if (drawBolt) {
			if (leftTarget) {
				for (int n = 0; n < numBolts; n++)
					renderBolt(last, mid, numPoints, fade, lPoints, startIndices[n], endIndices[n], widths, colors, 2, 8, 1);	
			}

			if (rightTarget) {
				for (int n = 0; n < numBolts; n++)
					renderBolt(last, mid, numPoints, fade, rPoints, startIndices[n], endIndices[n], widths, colors, 2, 8, 1);
			}
			// activate lighting when a larger bolt appears
			showLighting();
		}

		drawFloor();
		glDisable(GL_POLYGON_OFFSET_FILL);
		glPolygonOffset(0, 0);
		glDisable(GL_LIGHT0);
		glDisable(GL_LIGHT1);
	}
	
	/* Tracker updating functions: except for the code for updating particles, these are taken directly from the original code's update() function. */

	// adds the current position values of the figure to a Track container 
	void addFrame(Frame* f, ofxBvh *o, Track* track_) {
		for (int i = 0; i < o->getNumJoints(); i++)
		{
			const ofxBvhJoint *j = o->getJoint(i);

			for (int n = 0; n < j->getChildren().size(); n++)
			{
				f->push_back(j->getPosition());
				f->push_back(j->getChildren().at(n)->getPosition());
			}
		}
		track_->push_front(*f);	
		if (track_->size() > 200)
			track_->pop_back();
	}

	// applies gravity modifiers to the position data in Frames older than the current one; not currently used
	void modifyVertices() {
		// update vertexes flow
		for (int i = 0; i < track.size(); i++)
		{
			float delta = ofMap(i, 0, track.size(), 0, 1);
			Frame &f = track[i];
				
			for (int n = 0; n < f.size(); n++)
			{
				ofVec3f &v = f[n];
				ofVec3f f = 0;
					
				// gravity
				f.y -= -2.5 * (1 - sin(pow(delta, 2) * PI));
				f.y += ofNoise(v.y * 0.0001 + offset.y) * 1.4;
					
				f.x += ofSignedNoise(v.x * 0.0001 + offset.x) * 3;
				f.z += ofSignedNoise(v.z * 0.0001 + offset.z) * 3;
					
				if (v.y < 0)
				{
					f.y *= 0.02;
					if (n%2 == 0) {
						f.x *= 3;
						f.y *= 1.5;
						f.z *= 3;
					}
					else {
						f.x *= -3;
						f.y *= 1.5;
						f.z *= 3;
					}
				}
			
				v += f;
			}
		}
	}

	// stores the positions of the vertices in this figure's Track
	void cacheVertices() {
		// cache vertexes
			
		buffer.clear();
			
		for (int n = 0; n < 52; n += 2)
		{
			ofVec3f norm;
			BufferArray arr;

			for (int i = 0; i < track.size() - 1; i++)
			{
				float delta = ofMap(i, 0, track.size(), 0.1, 1);
				Frame &f1 = track[i];
					
				const ofVec3f &v1 = f1[n];
				const ofVec3f &v2 = f1[n + 1];
				const ofVec3f d = v1 - v2;
					
				const ofVec3f c1 = d.crossed(ofVec3f(0, 1, 0)).normalized();
				const ofVec3f c = c1.crossed(d).normalized();
				// if (c.y < 0) c *= -1;
					
				if (i == 0) norm.set(c);
					
				ofVec3f m = v1 * delta + v2 * (1 - delta);
				norm += (c - norm) * 0.3;
					
				Buffer buf;
					
				buf.norm = norm;
				buf.v1 = v1;
				buf.v2 = m;
					
				arr.push_back(buf);
			}
			buffer.push_back(arr);
		}
	}

	// emit particles around the heads of the figures and update properties of existing particles
	void handleParticles() {
		if (particleHandler.getSize() < 10000) {
			for (int j = 0; j < 8; j++) {
				ofVec3f next;
				// index 21 holds the position of the top of the figure
				next.x = track[0][21].x + rand()%20-10;
				next.y = track[0][21].y + rand()%20-10;
				next.z = track[0][21].z + rand()%20-10;
				particleHandler.emit(next, ofVec3f(rand()%1-1,0.5,rand()%1), 20, 1);
				next.x = track[0][21].x + rand()%4-2;
				next.y = track[0][21].y + rand()%4-2;
				next.z = track[0][21].z + rand()%4-2;
				particleHandler.emit(next, ofVec3f(0,0.5,0), 5, 1);
			}
		}
		particleHandler.updateParticles();
	}

	/* drawing functions */
	// draw an openGL point
	void drawPoint(int size, ofColor color, ofVec3f pos) {
		glPointSize(size);
		glBegin(GL_POINTS);
		ofSetColor(color);
		glVertex3fv(pos.getPtr());
		glEnd();
	}

	// draw an openGL line strip
	void drawLineStrip(int width, ofColor color, ofVec3f pos1, ofVec3f pos2) {
		glLineWidth(width);
		glBegin(GL_LINE_STRIP);
		ofSetColor(color);
		glVertex3fv(pos1.getPtr());
		glVertex3fv(pos2.getPtr());
		glEnd();
	}

	// draw the existing particles
	void drawParticles() {
		particleHandler.checkLifespans();
		vector<Particle> current = particleHandler.getParticles();
		for (int j = 0; j < current.size(); j++) {
			if (current[j].getType() == 1) {
				drawPoint(5, ofColor(230, 230, 230, 50), current[j].getPos());
				drawPoint(10, ofColor(70, 100, 200, 25), current[j].getPos());
				drawPoint(15, ofColor(70, 100, 200, 25), current[j].getPos());
			}
		}
	}

	// draws the figure this Tracker is handling.  Taken from the original code.
	void drawFigure() {
		if (!track.empty())
		{
			Frame &f = track[0];
			
			glLineWidth(1+rand()%3);
			ofSetColor(222, 222, 222, 120);
			glBegin(GL_LINES);
			for (int n = 0; n < f.size(); n += 2)
			{
				ofVec3f &v1 = f[n];
				ofVec3f &v2 = f[n + 1];
				
				glVertex3fv(v1.getPtr());
				glVertex3fv(v2.getPtr());
			}
			glEnd();
			// draw another set of lines for visual effect
			glLineWidth(10-rand()%2);
			ofSetColor(70, 120, 222, 100);
			glBegin(GL_LINES);
			for (int n = 0; n < f.size(); n += 2)
			{
				ofVec3f &v1 = f[n];
				ofVec3f &v2 = f[n + 1];
				
				glVertex3fv(v1.getPtr());
				glVertex3fv(v2.getPtr());
			}
			glEnd();
			// draw points at the joints of the figure
			for (int n = 0; n < f.size(); n++)
			{
				ofVec3f &v1 = f[n];
				drawPoint(10-rand()%2, ofColor(255, 255, 255, 55), v1);
			}
			for (int n = 0; n < f.size(); n++)
			{
				ofVec3f &v1 = f[n];
				drawPoint(15-rand()%2, ofColor(70, 120, 222, 100), v1);
			}
		}
	}

	/* generates values that will be used to draw randomized "lightning bolts" between points on this figure and one of the other figures
		Parameters: 
		int numPoints_: the number of connecting points to be used in drawing a bolt
		int boltTime_: the length of time the next bolts created will be displayed for */
	void setupBolts(int numPoints_, int boltTime_) {
		numBolts = rand()%2+1;
		numPoints = numPoints_;
		boltTime = boltTime_;
		placeCount = 0;
		segment = 0;
		// store randomized indices, which can be used to draw "lightning bolts" between random points on this figure and another one
		for (int i = 0; i < startPoints.size(); i++) {
			startIndices[i] = rand()%startPoints.size();
			endIndices[i] = rand()%startPoints.size();
		}
		if (rand()%2 == 0)
			direction = 1;
		else direction = -1;
		// store a set of "modifiers" that will be used to draw a jagged "lightning bolt"-like line by changing the y value of points along the line
		for (int i = 1; i <= numPoints; i++) {
			segment++;
			modifier[i] = 0;
			if (i != numPoints) {
				if (segment%(numPoints/3) == 0 && i != 1) { 
					direction *= -1;
					segment = 0;
					modifier[i] = (rand()%20)*direction;
				}
				else modifier[i] += (rand()%10)*direction;
				if (abs(modifier[i] > 100))
					modifier[i] = modifier[i-1];
			}
		}
	}

	// determines when "lightning bolts" will be drawn
	void handleBolts() {
		// random chance of determining that bolts should be drawn
		if (!drawBolt && rand()%80 == 0) {
			drawBolt = true;
			if (rand()%3 == 0) {
				leftTarget = true;
				rightTarget = false;
			}
			else if (rand()%3 == 1) {
				rightTarget = true;
				leftTarget = false;
			}
			else {
				leftTarget = true;
				rightTarget = true;
			}
		}
		// generate new information for a set of bolts
		if (boltTime <= 0) {
			setupBolts(20+rand()%4-2, numPoints+40+rand()%10);
		}
		else {
			boltTime--;
			// maintain a counter that controls "animation" of bolts
			if ((int)(boltTime)%2 == 0)
				placeCount+=2;
			if (boltTime <= 0) {
				drawBolt = false;
			}
		}
	}

	/* draws a "lightning bolt" as a series of line segments between random points determined in setupBolts().
		Parameters:
		ofVec3f last, mid: position vectors to store the locations of connecting points
		int numPoints_: number of connecting points
		int fade: modifier for alpha values when drawing lines, to make them fade away over time
		Frame target: the set of positions that this bolt can connect to
		int startIndex, endIndex: indices that determine which point in this figure will be the start of the bolt and which one in the target will be the endpoint 
		int widths[]: set of values to determine widths of the lines used for drawing the bolt
		ofColor colors[]: set of colors to use when drawing the bolt
		int sparkMod: determines chance of particles appearing at the start and end points of the bolt; higher causes a lower chance
		int intensity: determines how many overlapping lines to draw for the bolt - the bolt will appear brighter as this increases
		int positionMod: allows the position of the bolt to be changed by a factor if desired */
	void renderBolt(ofVec3f last, ofVec3f mid, int numPoints_, int fade, Frame target, int startIndex, int endIndex, int widths[], ofColor colors[], int sparkMod, int intensity, int positionMod) {
		for (int i = 1; i <= numPoints_; i++) {
			// emit particles where the bolt begins
			if (rand()%sparkMod == 0)
				particleHandler.emit(startPoints[startIndex], ofVec3f(rand()%2-1,rand()%2-1,rand()%2-1), 8, 1);
			// set the starting point for the first segment of the bolt
			if (i == 1) {
				last = startPoints[startIndex];
				last.y *= positionMod;
			}

			// determine the position of the next point to draw a line to, based on values generated in setupBolts()
			mid.x = startPoints[startIndex].x + float(i)/numPoints*(target[endIndex].x-startPoints[startIndex].x) + rand()%5;
			mid.y = startPoints[startIndex].y*positionMod + float(i)/numPoints*(target[endIndex].y-startPoints[startIndex].y)*positionMod + modifier[i] + rand()%5;
			mid.z = startPoints[startIndex].z + float(i)/numPoints*(target[endIndex].z-startPoints[startIndex].z) + rand()%5;

			// draw lines (multiple for visual effect) between the last point and the next point
			for (int j = 0; j < intensity; j++) {
				drawLineStrip(widths[0], colors[0], last, mid);
				drawLineStrip(widths[1], colors[1], last, mid);
				drawLineStrip(widths[2], colors[2], last, mid);
			}
			// set the endpoint of this line segment as the starting point for the next segment
			last = mid;
			// stop drawing segments when the bolt has not existed for very long - this "animates" the drawing of the bolt
			if (i > placeCount)
				break;
			// emit particles where the bolt ends if the bolt is close to its final point
			if (i > numPoints - 5 && rand()%sparkMod == 0) {
				for (int j = 0; j < 3; j++)
					particleHandler.emit(target[endIndex], ofVec3f(rand()%2-1,rand()%2-1,rand()%2-1), 8, 1);
			}
		}
	}

	// enable openGL lighting with a given set of parameters
	void showLighting() {
		glEnable(GL_COLOR_MATERIAL);
		glEnable(GL_LIGHT0);
		glEnable(GL_LIGHT1);

		// Create light components
		GLfloat lightPos[] = {0.0f, 50.0f, -50.0f, 0.0f};
		GLfloat lightPos1[] = {0.0f, 50.0f, -50.0f, 1.0f};
		GLfloat ambientLight[] = { 0.0f, 0.0f, 0.0f, 1.0f };
		GLfloat diffuseLight[] = { 0.0f, 0.0f, 0.0f, 0.5f };
		GLfloat specularLight[] = { 0.2f, 0.2f, 0.2f, 0.5f };
		GLfloat ambientLight1[] = { 0.0f, 0.0f, 0.0f, 1.0f };
		GLfloat diffuseLight1[] = { 0.2f, 0.2f, 0.2f, 1.0f };
		GLfloat specularLight1[] = { 0.2f, 0.2f, 0.2f, 1.0f };
		GLfloat direction[] = {0, -1, 0};

		// Assign created components to lights
		glLightfv(GL_LIGHT0, GL_POSITION, lightPos);
		glLightfv(GL_LIGHT0, GL_AMBIENT, ambientLight);
		glLightfv(GL_LIGHT0, GL_DIFFUSE, diffuseLight);
		glLightfv(GL_LIGHT0, GL_SPECULAR, specularLight);
		glLightf(GL_LIGHT0, GL_SPOT_CUTOFF, 100.0);
		glLightfv(GL_LIGHT0, GL_SPOT_DIRECTION, direction);
		glLightf(GL_LIGHT0, GL_SPOT_EXPONENT, 2.0);

		glLightfv(GL_LIGHT1, GL_POSITION, lightPos);
		glLightfv(GL_LIGHT1, GL_AMBIENT, ambientLight1);
		glLightfv(GL_LIGHT1, GL_DIFFUSE, diffuseLight1);
		glLightfv(GL_LIGHT1, GL_SPECULAR, specularLight1);
	}

	// draw a plane at y = 0 to show the effect of lighting
	void drawFloor() {
		ofSetColor(10, 10, 10, 10);
		float mcolor[] = { 1.0f, 0.0f, 0.0f, 1.0f };
		float specReflection[] = {1.0f, 1.0f, 1.0f};
		float diffuse[] = {1.0f, 1.0f, 1.0f};
		float ambient[] = {0.0f, 0.0f, 0.0f};
		glMaterialfv(GL_FRONT, GL_SPECULAR, specReflection);
		
		glMaterialfv(GL_FRONT, GL_AMBIENT, ambient);
		glMaterialfv(GL_FRONT, GL_DIFFUSE, diffuse);

		glBegin(GL_QUADS);
		glVertex3f(-10000.0f, 0.0f, 10000.0f);              // Top Left
        glVertex3f(10000.0f, 0.0f, 10000.0f);              // Top Right
        glVertex3f(10000.0f, 0.0f, -10000.0f);              // Bottom Right
		glVertex3f(-10000.0f, 0.0f, -10000.0f);             // Bottom Left            
		glNormal3fv(ofVec3f(0.0f, 1.0f, 0.0f).getPtr());
		glEnd();	
	}
	
};

/* functions for running the program in general; nearly all from the original code except for adding more figures to the bvh vector */
//--------------------------------------------------------------
void testApp::setup()
{
	ofSetFrameRate(60);
	ofSetVerticalSync(true);
	
	ofSetSmoothLighting(true);
	ofSetGlobalAmbientColor(ofColor(220));
	glEnable(GL_LIGHTING);

	ofBackground(10);
	
	bvh.resize(3);
	
	// You have to get motion and sound data from http://www.perfume-global.com
	
	// setup bvh
	bvh[0].load("bvhfiles/aachan.bvh");
	bvh[1].load("bvhfiles/kashiyuka.bvh");
	bvh[2].load("bvhfiles/nocchi.bvh");

	for (int i = 0; i < bvh.size(); i++)
	{
		bvh[i].setFrame(4);
	}
	
	track.loadSound("Perfume_globalsite_sound.wav");
	track.setLoop(true);
	track.play();
	
	// setup tracker
	for (int i = 0; i < bvh.size(); i++)
	{
		ofxBvh &b = bvh[i];

		Tracker *t = new Tracker;
		t->setup(&b, i);
		trackers.push_back(t);
	}
	
	trackers[0]->setBvhL(&bvh[2]);
	trackers[1]->setBvhL(&bvh[0]);
	trackers[2]->setBvhL(&bvh[1]);
	trackers[0]->setBvhR(&bvh[1]);
	trackers[1]->setBvhR(&bvh[2]);
	trackers[2]->setBvhR(&bvh[0]);

	offset.x = ofRandom(1);
	offset.y = ofRandom(1);
	offset.z = ofRandom(1);
	offset_v.x = ofRandom(0.001);
	offset_v.y = ofRandom(0.005);
	offset_v.z = ofRandom(0.001);
	
	campos_t.set(0, 0, -300);
}

//--------------------------------------------------------------
void testApp::update()
{
	float t = (track.getPosition() * trackDuration);
	t = t / bvh[0].getDuration();
	
	center_t.set(0, 0, 0);
	
	for (int i = 0; i < bvh.size(); i++)
	{
		bvh[i].setPosition(t);
		bvh[i].update();
		
		center_t += bvh[i].getJoint(0)->getPosition();
	}
	
	center_t /= 3;
	center += (center_t - center) * 0.01;
	
	for (int i = 0; i < trackers.size(); i++)
	{
		trackers[i]->update();
	}
	
	offset += offset_v;
	
	cam.setPosition(campos.x, campos.y, campos.z);
	cam.lookAt(ofVec3f(0, 0, 0));
	campos += (campos_t - campos) * 0.01;
}

//--------------------------------------------------------------
void testApp::draw(){
	glDisable(GL_DEPTH_TEST);
	glShadeModel(GL_SMOOTH);
	
	ofEnableSmoothing();
	
	glEnable(GL_LINE_SMOOTH);
	glHint(GL_LINE_SMOOTH_HINT, GL_NICEST);
	glLineWidth(1);

	// smooth particle
	glEnable(GL_POINT_SMOOTH);
	glHint(GL_POINT_SMOOTH_HINT, GL_NICEST);
	
	ofEnableBlendMode(OF_BLENDMODE_ADD);

	//light.enable();
	light.setPosition(0, -500, 0);
	
	cam.begin();
	
	ofPushMatrix();
	{
		glRotatef(ofGetElapsedTimef() * 20, 0, 1, 0);
		glTranslatef(-center.x, -100, -center.z);
		
		ofSetColor(50);

		// draw position markers on the y = 0 plane
		/*for (int x = -10; x < 10; x++)
		{
			for (int y = -10; y < 10; y++)
			{
				ofPushMatrix();
				glTranslatef(x * 500, 0, y * 500);
				ofLine(10, 0, 0, -10, 0, 0);
				ofLine(0, 0, 10, 0, 0, -10);
				ofPopMatrix();
			}
		}*/
		
		ofSetColor(ofColor::white, 80);
		for (int i = 0; i < trackers.size(); i++)
		{
			trackers[i]->draw();
		}
	}
	ofPopMatrix();
	
	cam.end();
	
	light.disable();
	
}

//--------------------------------------------------------------
void testApp::keyPressed(int key){
	campos_t.x = ofRandom(-600, 600);
	campos_t.z = ofRandom(-600, 600);
	campos_t.y = ofRandom(-100, 200);
}

//--------------------------------------------------------------
void testApp::keyReleased(int key){
	
}

//--------------------------------------------------------------
void testApp::mouseMoved(int x, int y ){

}

//--------------------------------------------------------------
void testApp::mouseDragged(int x, int y, int button){

}

//--------------------------------------------------------------
void testApp::mousePressed(int x, int y, int button){

}

//--------------------------------------------------------------
void testApp::mouseReleased(int x, int y, int button){
}

//--------------------------------------------------------------
void testApp::windowResized(int w, int h){
}

//--------------------------------------------------------------
void testApp::gotMessage(ofMessage msg){

}

//--------------------------------------------------------------
void testApp::dragEvent(ofDragInfo dragInfo){ 
}
