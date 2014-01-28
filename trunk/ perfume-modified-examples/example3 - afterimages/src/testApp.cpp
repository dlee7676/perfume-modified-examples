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
	deque<Frame> track, lTrack, rTrack;
	int numPoints;
	bool drawClone;
	
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
		id = id_;
		drawClone = false;
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
			Frame f;
			addFrame(&f, bvh, &track);
			modifyVertices();
			cacheVertices();
			particleHandler.updateParticles();
		}
	}

	// draw the elements in the scene
	void draw()
	{
		glEnable(GL_POLYGON_OFFSET_FILL);
		glPolygonOffset(1, 1);
		setupParticles();
		drawParticles();
		drawFigure();
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

	// periodically emit particles at the figure's joints - these will form an "afterimage"
	void setupParticles() {
		if (int(ofGetElapsedTimef())%2 == 0) {
			// track[0] stores the current frame's position data
			for (int j = 0; j < track[0].size(); j++) {
				if (drawClone && particleHandler.getSize() < 5000) {
					particleHandler.checkLifespans();
					particleHandler.emit(track[0][j], ofVec3f(0,0,0), 300, 0);
				}
			}
			drawClone = false;
		}

		if (int(ofGetElapsedTimef())%2 != 0) {
			drawClone = true;
		}
	}

	// draw the existing particles and change their properties according to their lifespan
	void drawParticles() {
		vector<Particle> current = particleHandler.getParticles();
		for (int j = 0; j < current.size(); j+=2) {
			if (current[j].getType() == 0) {
				int size, fade;
				// modify the size and transparency of the particles as their lifespan decreases
				if (current[j].getLifespan() > 288) {
					size = (300 - current[j].getLifespan())*3;
					fade = 0;
				}
				else {
					size = 0;
					fade = (300 - current[j].getLifespan()) / 2;
				}
				// draw several particles at each particle position for visual effect
				glPointSize(3+size);
				glBegin(GL_POINTS);
				ofSetColor(230, 230, 230, 150-fade);
				glVertex3fv(current[j].getPos().getPtr());
				glVertex3fv(current[j+1].getPos().getPtr());
				glEnd();
				glPointSize(9+size);
				glBegin(GL_POINTS);
				ofSetColor(100, 100, 100, 100-fade);
				glVertex3fv(current[j].getPos().getPtr());
				glVertex3fv(current[j+1].getPos().getPtr());
				glEnd();
				glPointSize(15+size);
				glBegin(GL_POINTS);
				// change color of the largest particles based on which figure they come from
				if (id == 0)
					ofSetColor(150, 100, 100, 100-fade);
				if (id == 1)
					ofSetColor(100, 150, 100, 100-fade);
				if (id == 2)
					ofSetColor(150, 150, 70, 100-fade);
				glVertex3fv(current[j].getPos().getPtr());
				glVertex3fv(current[j+1].getPos().getPtr());
				glEnd();
				// connect the particles with lines, to make a copy of the figure as it looked in this frame
				glLineWidth(2);
				glBegin(GL_LINES);
				if (j > 0) {
					glVertex3fv(current[j].getPos().getPtr());
					glVertex3fv(current[j+1].getPos().getPtr());
				}
				glEnd();
			}
		}
	}

	// draws the figure this Tracker is handling.  Taken from the original code.
	void drawFigure() {
		if (!track.empty())
		{
			Frame &f = track[0];
			
			glLineWidth(2);
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
			// draw another set of lines for visual effect; color changes depending on which figure they correspond to
			glLineWidth(20);
			ofSetColor(70, 120, 222, 100);
			if (id == 0)
				ofSetColor(200, 70, 70, 100);
			if (id == 1)
				ofSetColor(70, 150, 70, 100);
			if (id == 2)
				ofSetColor(200, 200, 70, 100);
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
			glPointSize(rand()%10+10);
			glBegin(GL_POINTS);
			for (int n = 0; n < f.size(); n++)
			{
				ofVec3f &v1 = f[n];
				glVertex3fv(v1.getPtr());
			}
			glEnd();
			for (int n = 0; n < f.size(); n++)
			{
				ofVec3f &v1 = f[n];
				drawPoint(10-rand()%2, ofColor(255, 255, 255, 55), v1);
			}
		}
	}

	// enable openGL lighting with a given set of parameters; not used in this version
	void showLighting() {
		glEnable(GL_LIGHTING);
		glEnable(GL_COLOR_MATERIAL);
		glEnable(GL_LIGHT0);
		glEnable(GL_LIGHT1);

		// Create light components
		GLfloat lightPos[] = {0.0f, 50.0f, -50.0f};
		GLfloat ambientLight[] = { 1.0f, 1.0f, 1.0f, 1.0f };
		GLfloat diffuseLight[] = { 1.0f, 1.0f, 1.0f, 1.0f };
		GLfloat specularLight[] = { 0.3f, 0.3f, 0.3f, 0.3f };
		GLfloat ambientLight1[] = { 0.2f, 0.2f, 0.2f, 0.2f };
		GLfloat specularLight1[] = { 1.0f, 1.0f, 1.0f, 1.0f };
		GLfloat direction[] = {0, -1, 0};

		// Assign created components to lights
		glLightfv(GL_LIGHT0, GL_POSITION, lightPos);
		glLightfv(GL_LIGHT0, GL_AMBIENT, ambientLight);
		glLightfv(GL_LIGHT0, GL_DIFFUSE, diffuseLight);
		glLightfv(GL_LIGHT0, GL_SPECULAR, specularLight);

		glLightfv(GL_LIGHT1, GL_POSITION, lightPos);
		glLightfv(GL_LIGHT1, GL_AMBIENT, ambientLight1);
		glLightfv(GL_LIGHT1, GL_DIFFUSE, diffuseLight);
		glLightfv(GL_LIGHT1, GL_SPECULAR, specularLight1);
		glLightf(GL_LIGHT1, GL_SPOT_CUTOFF, 100.0);
		glLightfv(GL_LIGHT1, GL_SPOT_DIRECTION, direction);
		glLightf(GL_LIGHT1, GL_SPOT_EXPONENT, 2.0);
	}

	// draw a plane at y = 0 to show the effect of lighting
	void drawFloor() {
		ofSetColor(10, 10, 10, 10);
		float mcolor[] = { 1.0f, 0.0f, 0.0f, 1.0f };
		float specReflection[] = {1.0f, 1.0f, 1.0f};
		float diffuse[] = {1.0f, 1.0f, 1.0f};
		float ambient[] = {0.5f, 0.5f, 0.5f};
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
