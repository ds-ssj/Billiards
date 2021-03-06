#include "background.h"
#include "draw.h"
#include "entity.h"
#include "sphere.h"
#include <cmath>

Billard billard;	// game logic
GLUquadricObj *ballQuadric;		// used to render quadric(sphere, cylinder...)
GLUquadricObj *skyQuadric;		// used to render sky dome

Point camera = { 0.0f, 5.0f, 5.0f };
Point view;

/* Used for banner and flags */
GLfloat PI = 4 * atan(1);
const GLint T = 1000;		// Period of flags' animation
#define UNUM 15		// used by Bezier in u direction (control points)
#define VNUM 10		// used by Bezier in v direction (control points)
#define FLAG_LENGTH 1.2f
#define FLAG_WIDTH 1.2f
GLfloat ctrlpoints[UNUM][VNUM][3] = { { 0, 0, 0 } };	// Bezier control points
GLfloat texpts[2][2][2] = { { { 0.0,0.0 },{ 1.0,0.0 } },{ { 0.0, 1.0 },{ 1.0,1.0 } } };	// texture for Bezier

#define BANNER_UNUM		45	// points of banner in u direction
#define BANNER_VNUM		45	// points of banner in v direction
#define BANNER_LENGTH	2.0f	// length of banner
#define BANNER_WIDTH	1.2f	// width of banner
GLfloat bannerPoints[BANNER_UNUM][BANNER_VNUM][3];	// for banner

/* Used for Terrain */
const unsigned STEP_SIZE = 8;
BYTE terrainHeight[MAP_X][MAP_Y];

/* For light */
GLboolean light = TRUE;

/* return the height */
inline GLfloat getHeight(GLfloat x, GLfloat z) {
	float nx = (x+20.0f) / 0.04f;		// before scaled
	float ny = (z+20.0f) / 0.04f;
	int i = int(nx);
	int j = int(ny);
	float x1 = (terrainHeight[i + 1][j] - terrainHeight[i][j]) * (nx - i) + terrainHeight[i][j];
	float x2 = (terrainHeight[i + 1][j + 1] - terrainHeight[i][j + 1]) * (ny - j) + terrainHeight[i][j + 1];
	float y0 = (x2 - x1) * (ny - j) + x1;

	return (y0 * 0.04 * 0.8 - 5.0);
}

/* Some initialization of whole scene (use only once) */
GLint GLhandlers::initalHandler() {									// All Setup For OpenGL Goes Here
	if (!loadGLTexture(textures))  return FALSE;
	glEnable(GL_TEXTURE_2D);							// Enable 2D Textures

	glShadeModel(GL_SMOOTH);							// Enable Smooth Shading
	glClearColor(0.5f, 0.5f, 0.5f, 0.5f);				// Grey Background
	glClearDepth(1.0f);									// Depth Buffer Setup
	glEnable(GL_DEPTH_TEST);							// Enables Depth Testing
	glDepthFunc(GL_LEQUAL);								// The Type Of Depth Testing To Do
	glHint(GL_PERSPECTIVE_CORRECTION_HINT, GL_NICEST);	// Really Nice Perspective Calculations
	glEnable(GL_LIGHTING);								// Enable Lighting
	glEnable(GL_MAP2_VERTEX_3);							// Enable 2D-Evaluator
	glEnable(GL_MAP2_TEXTURE_COORD_2);					// Enable 2D-Texture

	// For Bezier Curved Surface Animation
	for (int i = 0; i < UNUM; i++) {
		for (int j = 0; j < VNUM; j++) {
			ctrlpoints[i][j][1] = -i / double(UNUM) * FLAG_LENGTH;
			ctrlpoints[i][j][2] = -j / double(VNUM) * FLAG_WIDTH;
		}
	}

	// initial banner control points
	for (int i = 0; i < BANNER_UNUM; i++) {
		for (int j = 0; j < BANNER_VNUM; j++) {
			bannerPoints[i][j][0] = (i * BANNER_LENGTH / BANNER_UNUM) - BANNER_LENGTH / 2;
			bannerPoints[i][j][1] = (j * BANNER_WIDTH / BANNER_VNUM) - BANNER_WIDTH / 2;
			bannerPoints[i][j][2] = 0.5 * sin(((i * 8.0f / 360.0f)*PI*2));
		}
	}

	// initial height for terrain
	loadRawFile("Data/Terrain.raw", MAP_X * MAP_Y, (BYTE *)terrainHeight);

	// Instantiates the quadric object used to render the billiard balls.
	ballQuadric = gluNewQuadric();
	gluQuadricNormals(ballQuadric, GLU_SMOOTH);
	gluQuadricOrientation(ballQuadric, GLU_OUTSIDE);
	gluQuadricTexture(ballQuadric, GL_TRUE);

	skyQuadric = gluNewQuadric();
	gluQuadricNormals(skyQuadric, GLU_SMOOTH);
	gluQuadricOrientation(skyQuadric, GLU_INSIDE);
	gluQuadricTexture(skyQuadric, GL_TRUE);

	return TRUE;										// Initialization Went OK
}

GLvoid GLhandlers::renderSky() {
	GLdouble eqn[4] = { 0.0, -1.0, 0.0, 0.0 };
	static float angle = 0.0f;
	glPushMatrix();
	glRotatef(180.0f, 1.0f, 0.0f, 0.0f);
	glRotatef(angle, 0.0f, 1.0f, 0.0f);
	glTranslatef(0.0f, 5.0f, 0.0f);
	glClipPlane(GL_CLIP_PLANE0, eqn);
	glEnable(GL_CLIP_PLANE0);
	glBindTexture(GL_TEXTURE_2D, textures[TEXTURE_SKY]);
	gluSphere(skyQuadric, 20.0f, 32, 32);
	glBindTexture(GL_TEXTURE_2D, 0);		// clean the texture
	glPopMatrix();
	angle += 0.005f;
}

GLvoid GLhandlers::renderTerrain() {
	int x = 0.0f, y = 0.0f, z = 0.0f;
	static float angle = 0.0f;
	float scaleValue = 0.04f;

	glPushMatrix();
	glTranslatef(-20.0f, -5.0f, -20.0f);
	glScalef(scaleValue, scaleValue * 0.8, scaleValue);

	glBindTexture(GL_TEXTURE_2D, textures[TEXTURE_GROUND]);
	for (int i = 0; i < MAP_X; i += STEP_SIZE) {
		for (int j = 0; j < MAP_Y; j += STEP_SIZE) {
			
			Point a = { (float)i, (float)terrainHeight[i][j] , (float)j };
			Point b = { (float)i, (float)terrainHeight[i][j + STEP_SIZE], float(j + STEP_SIZE) };
			Point c = { float(i + STEP_SIZE), (float)terrainHeight[i + STEP_SIZE][j + STEP_SIZE], float(j + STEP_SIZE) };
			Point d = { float(i + STEP_SIZE), (float)terrainHeight[i + STEP_SIZE][j], (float)j };
			Point normal;

			glBegin(GL_QUADS);
			// Point(i, j)
			normal = (b - a) * (d - a);
			normal /= normal.norm();
			glNormal3f(normal.x, normal.y, normal.z);
			glTexCoord2f(0.0f, 0.0f); glVertex3i(a.x, a.y, a.z);

			// Point(i, j + 1)
			normal = (c - b) * (a - b);
			normal /= normal.norm();
			glNormal3f(normal.x, normal.y, normal.z);
			glTexCoord2f(0.0f, 1.0f); glVertex3i(b.x, b.y, b.z);

			// Point(i + 1, j + 1)
			normal = (d - c) * (b - c);
			normal /= normal.norm();
			glNormal3f(normal.x, normal.y, normal.z);
			glTexCoord2f(1.0f, 1.0f); glVertex3i(c.x, c.y, c.z);

			// Point(i + 1, j)
			normal = (a - d) * (c - d);
			normal /= normal.norm();
			glNormal3f(normal.x, normal.y, normal.z);
			glTexCoord2f(1.0f, 0.0f); glVertex3i(d.x, d.y, d.z);		
			glEnd();
		}
	}
	glBindTexture(GL_TEXTURE_2D, 0);		// clean the texture
	glPopMatrix();
}

/* Render 16 balls on table */
GLvoid GLhandlers::renderBall() {
	Ball *ball = billard.getBall(0);
	view.x = ball->pos.x;
	view.y = ball->pos.y + getHeight(ball->pos.x, ball->pos.z);//ball->pos[Y];
	view.z = ball->pos.z;

	for (int i = 0; i < 16; i++) {
		ball = billard.getBall(i);
		float x = ball->pos.x,
			y = ball->pos.y + getHeight(ball->pos.x, ball->pos.z), // ball->pos[Y],
			z = ball->pos.z,
			r = ball->rad;
		glPushMatrix();
			glTranslatef(x, y, z);
			if (i != 8) {
				glBindTexture(GL_TEXTURE_2D, textures[TEXTURE_BALL_BASE + i]);
				gluSphere(ballQuadric, r, 32, 32);	// SolidSphere cannot show all texture
			} else {
				drawSphere(r, textures[TEXTURE_BALL_BASE + i], 50, 50);
			}

		glBindTexture(GL_TEXTURE_2D, 0);		// clean the texture
		glBindTexture(GL_TEXTURE_3D, 0);		// clean the texture
		glPopMatrix();
	}

}

/* Render flags */
GLvoid GLhandlers::renderFlag(GLfloat x, GLfloat z, GLint texture) {

	static int t = 0;
	glPushMatrix();
		glColor3f(1.0, 0.0, 0.0);
		glRotated(90.0, 1.0, 0.0, 0.0);
		glTranslated(x, z, -getHeight(x, z)-2.0);
		gluCylinder(ballQuadric, 0.03f, 0.03f, 3.43f, 8, 1);

		/* For Animation */
		for (int i = 0; i < UNUM; i++) {
			for (int j = 0; j < VNUM; j++) {
				ctrlpoints[i][j][0] = 
					sin(2 * PI*(-2.5 * i / 14.0f + t / float(T))) - sin(2 * PI* t / float(T));
				ctrlpoints[i][j][0] *= 0.2;
			}
		}
		t = (t + 1) % T;
		glMap2f(GL_MAP2_VERTEX_3,		// Bezier
			0.0, 1.0, 3*VNUM, UNUM,
			0.0, 1.0, 3, VNUM,
			&ctrlpoints[0][0][0]);
		glMap2f(GL_MAP2_TEXTURE_COORD_2,	// Texture
				0.0, 1.0, 2, 2, 
				0.0, 1.0, 4, 2, 
				&texpts[0][0][0]);
		glMapGrid2f(60, 0.0, 1.0, 20, 0.0, 1.0);

		/* draw */
		glColor3f(1.0, 1.0, 1.0);
		glTranslated(0.0f, 0.0, 1.2f);
		glBindTexture(GL_TEXTURE_2D, textures[texture]);
		glEvalMesh2(GL_FILL, 0, 60, 0, 20);
		glBindTexture(GL_TEXTURE_2D, 0);		// clean the texture

	glPopMatrix();
}

GLvoid GLhandlers::renderBanner(GLfloat x, GLfloat y, GLfloat z, GLint texture) {
	glPushMatrix();
		glTranslatef(x, y, z);
		glBindTexture(GL_TEXTURE_2D, textures[texture]);

		// draw each facet
		// facet num = control_point_num - 1 (in each direction)
		glBegin(GL_QUADS);
		for (int i = 0; i < BANNER_UNUM - 1; i++) {		
			for (int j = 0; j < BANNER_VNUM - 1; j++) {
				/* Used for texture*/
				float u1 = float(i) / (BANNER_UNUM - 1),
					v1 = float(j) / (BANNER_VNUM - 1),
					u2 = float(i + 1) / (BANNER_UNUM - 1),
					v2 = float(j + 1) / (BANNER_VNUM - 1);

				glNormal3f(0.0f, 0.0f, 1.0f);
				glTexCoord2f(u1, v1);	// texture
				glVertex3f(bannerPoints[i][j][0], bannerPoints[i][j][1], bannerPoints[i][j][2]);	// small facet

				glTexCoord2f(u1, v2);
				glVertex3f(bannerPoints[i][j + 1][0], bannerPoints[i][j + 1][1], bannerPoints[i][j + 1][2]);

				glTexCoord2f(u2, v2);
				glVertex3f(bannerPoints[i + 1][j + 1][0], bannerPoints[i + 1][j + 1][1], bannerPoints[i + 1][j + 1][2]);

				glTexCoord2f(u2, v1);
				glVertex3f(bannerPoints[i + 1][j][0], bannerPoints[i + 1][j][1], bannerPoints[i + 1][j][2]);
			}
		}
		glEnd();
		glBindTexture(GL_TEXTURE_2D, 0);		// clean the texture
	glPopMatrix();

	/* Animation */
	static int t = 0;
	if (t == 10)		// 10 is a period
	{
		for (int j = 0; j < BANNER_VNUM; j++) {
			float hold = bannerPoints[BANNER_UNUM - 1][j][2];
			for (int i = BANNER_UNUM - 1; i > 0; i--) {
				bannerPoints[i][j][2] = bannerPoints[i - 1][j][2];
			}
			bannerPoints[0][j][2] = hold;
		}
		t = 0;
	}
	t++;
}

GLvoid dealKeys(GLvoid) {
	/* change angle of camera */
	// if (keys[VK_UP])  xAngle += (xAngle > 180.0f) ? 0.0f : 0.2f;
	// if (keys[VK_DOWN]) xAngle -= (xAngle < -180.0f) ? 0.0f : 0.2f;
	// if (keys[VK_LEFT]) yAngle += (yAngle > 360.0f) ? -360.0f : 0.2f;
	// if (keys[VK_RIGHT]) yAngle -= (yAngle < 0.0f) ? -360.0f : 0.2f;

	/* change location of camera*/
	if (keys['J']) camera.x -= (camera.x < -5.0f) ? 0.0f : 0.01f;
	if (keys['L']) camera.x += (camera.x > 5.0f) ? 0.0f : 0.01f;
	if (keys['U']) camera.y -= (camera.y < -2.0f) ? 0.0f : 0.01f;
	if (keys['O']) camera.y += (camera.y > 5.0f) ? 0.0f : 0.01f;
	if (keys['I']) camera.z -= (camera.z < -10.0f)? 0.0f : 0.01f;
	if (keys['M']) camera.z += (camera.z > 5.0f) ? 0.0f : 0.01f;
	if (keys['K']) camera = { 0.0f, 5.0f, 5.0f }; // reset
		
	/* Player Controller */
	Point accDir;
	Ball *mother = billard.getBall(0);
	if (keys['W']) {
		accDir = mother->pos - camera;
		accDir.y = 0.0f;
		billard.shoot(accDir);
	}
	if (keys['S']) {
		accDir =  camera - mother->pos;
		accDir.y = 0.0f;
		billard.shoot(accDir);
	}
	if (keys['A']) {
		accDir.x = mother->pos.z - camera.z;
		accDir.y = 0.0f;
		accDir.z = camera.x - mother->pos.x;
		billard.shoot(accDir);
	}
	if (keys['D']) {
		accDir.x = camera.z - mother->pos.z;
		accDir.y = 0.0f;
		accDir.z = mother->pos.x - camera.x;
		billard.shoot(accDir);
	}

	if (keys['G'] && light) {
		light = FALSE;
	}
	if (!keys['G']) {
		light = TRUE;
	}
}

GLint GLhandlers::drawHandler() {								// Here's Where We Do All The Drawing
	glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);	// Clear Screen And Depth Buffer
	glLoadIdentity();	// Reset The Current Modelview Matrix

	dealKeys();
	std::vector<Point> crashPoints = billard.updateBalls();

	/* mother ball */
	Ball *mb = billard.getBall(0);
	/* Lighting */
	GLfloat LightAmbient[] = { 1.0f, 1.0f, 1.0f, 1.0f };	// Ambient Light
	GLfloat LightDiffuse[] = { 1.0f, 1.0f, 1.0f, 1.0f };	// Diffuse Light
	GLfloat LightPosition[] = { 0.0f, 0.0f, 3.0f, 1.0f };	// postion of light
	if (light) {
		glLightfv(GL_LIGHT0, GL_AMBIENT, LightAmbient);
		glLightfv(GL_LIGHT0, GL_DIFFUSE, LightDiffuse);
		glLightfv(GL_LIGHT0, GL_POSITION, LightPosition);
		glEnable(GL_LIGHT0);
	} else {
		glDisable(GL_LIGHT0);
	}

	/* Spotlight */
	GLfloat SpotLightDiffuse[] = { 0.0f, 10.0f, 0.0f, 1.0f };
	GLfloat SpotLightPosition[] = { view.x, view.y + 1.0f, view.z, 1.0f };
	GLfloat SpotLightDirection[] = { 0.0f, -1.0f, 0.0f };

	/* Render */
	glPushMatrix();
		gluLookAt(camera.x, camera.y, camera.z, view.x, view.y, view.z, 0.0, 1.0, 0.0);
		glTranslatef(0.0f, 0.0f, 1.0f);
		
		if (!light) {
			glLightf(GL_LIGHT1, GL_SPOT_CUTOFF, 30.0);
			glLightfv(GL_LIGHT1, GL_DIFFUSE, SpotLightDiffuse);
			glLightfv(GL_LIGHT1, GL_POSITION, SpotLightPosition);
			glLightfv(GL_LIGHT1, GL_SPOT_DIRECTION, SpotLightDirection);
			glEnable(GL_LIGHT1);
		} else {
			glDisable(GL_LIGHT1);
		}

		renderTerrain();
		renderSky();
		renderFlag(-2.0f, 0.0f, TEXTURE_FLAG_2);
		renderFlag(2.0f, -LENGTH, TEXTURE_FLAG_1);
		renderBanner(0.0f, 3.0f, -5.0f, TEXTURE_TITLE);
		part.draw(view, mb->vel, textures[TEXTURE_PARTICLE]);
		part.crash(textures[TEXTURE_PARTICLE]);
		for (int i = 0; i < crashPoints.size(); i++) {
			Point p = crashPoints[i];
			p.y = getHeight(p.x, p.z);
			part.crashInit({0.0f, 0.0f, 0.0f});
		}
		renderBall();		
	glPopMatrix();
	
	glFlush();
	return TRUE;
}

//GLfloat stickAngle = 0.0f;	// angle offset of cue stick in z direction
//GLfloat stickPower = 1.0f;

/* Old Version : Deal with key events */
//GLvoid updateBillard(GLvoid) {
//	billard.updateBalls();
//	static GLint shoot = -1;	// is shoot ?
//	billard.updateStick(stickAngle, stickPower);
//	if (shoot >= 0) {
//		billard.shoot(shoot);
//		stickPower = MIN_POWER;
//		stickAngle = 0.0f;
//	}
//}

/* Render the surface of table */
//GLvoid GLhandlers::renderTable() {
//	glBindTexture(GL_TEXTURE_2D, textures[TEXTURE_TABLE]);
//	glBegin(GL_QUADS);
//	{
//		// Back
//		glVertex3f(-WIDTH/2, 0.0f, 0.0f);		// bottom-left
//		glVertex3f(WIDTH/2, 0.0f, 0.0f);		// bottom-right
//		glVertex3f(WIDTH/2, THICKNESS, 0.0f);		// upper-right
//		glVertex3f(-WIDTH/2, THICKNESS, 0.0f);		// upper-left
//
//		// Right
//		glVertex3f(WIDTH / 2, THICKNESS, 0.0f);		// upper-left
//		glVertex3f(WIDTH / 2, 0.0f, 0.0f);		// bottom-left
//		glVertex3f(WIDTH / 2, 0.0f, -LENGTH);		// bottom-right
//		glVertex3f(WIDTH / 2, THICKNESS, -LENGTH);		// upper-right
//
//		// Front
//		glVertex3f(WIDTH / 2, 0.0f, -LENGTH);		// bottom-left
//		glVertex3f(WIDTH / 2, THICKNESS, -LENGTH);		// upper-left
//		glVertex3f(-WIDTH / 2, THICKNESS, -LENGTH);		// upper-right
//		glVertex3f(-WIDTH / 2, 0.0f, -LENGTH);		// bottom-right
//
//		// Left
//		glVertex3f(-WIDTH / 2, 0.0f, 0.0f);		// bottom-right
//		glVertex3f(-WIDTH / 2, THICKNESS, 0.0f);		// upper-right
//		glVertex3f(-WIDTH / 2, THICKNESS, -LENGTH);		// upper-left
//		glVertex3f(-WIDTH / 2, 0.0f, -LENGTH);		// bottom-left
//
//		// Top
//		glTexCoord2f(0.0f, 0.0f);  glVertex3f(-WIDTH/2, THICKNESS, 0.0f);		// bottom-left
//		glTexCoord2f(1.0f, 0.0f);  glVertex3f(WIDTH/2, THICKNESS, 0.0f);		// bottom-right
//		glTexCoord2f(1.0f, 1.0f);  glVertex3f(WIDTH/2, THICKNESS, -LENGTH);		// upper-right
//		glTexCoord2f(0.0f, 1.0f);  glVertex3f(-WIDTH/2, THICKNESS, -LENGTH);		// upper-left
//	}
//	glEnd();
//}

/* Render cue stick */
//GLvoid GLhandlers::renderCueStick() {
//	Ball *cueBall = billard.getBall(0);		// get cue ball
//	CueStick *cueStick = billard.getStick();
//
//	float r = cueBall->rad + cueStick->power/10;
//	float x = cueBall->pos[X] - r * cueStick->direction[0],
//		y = cueBall->pos[Y] - r * cueStick->direction[1],
//		z = cueBall->pos[Z] - r * cueStick->direction[2];
//	glPushMatrix();
//		glTranslatef(x, y, z);
//		glRotatef(stickAngle, 0.0f, 1.0f, 0.0f);
//		glBindTexture(GL_TEXTURE_2D, textures[TEXTURE_CUESTICK]);
//		gluCylinder(ballQuadric, 0.03f, 0.05f, 3.43f, 8, 1);
//	glPopMatrix();
//}