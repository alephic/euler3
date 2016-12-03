/*
 * EULER3 Differential Equation Grapher
 * Luke Nelson and Kaj Bostrom
 */

#include <GLFW/glfw3.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <time.h>
#include <unistd.h>
#include <assert.h>
#include <pthread.h>

#define POINT_COUNT 1048576/4
#define POINTS_PER_BATCH (1048576/128)
#define THREAD_COUNT 8
#define DO_BATCHES
#define LAG 1
#define RECALCULATE_COLOR
//#define LINES
#define THREEDEE
#define TIME_STEP 0.02
#define MOTION_SCALE 0.02
#define Q_STEP 0.02

#ifndef THREEDEE
	#define dx(x,y) y
	#define dy(x,y) -x

#else
	#define dx(x,y,z) sin(x)*sin(z+q)
	#define dy(x,y,z) sin(y)*sin(x+q)
	#define dz(x,y,z) sin(z)*sin(y+q)

	#define ddx(x,y,z) 0.0
	#define ddy(x,y,z) 0.0
	#define ddz(x,y,z) 0.0
#endif

static float t = 0.0;
static float q = 0.0;

static unsigned char left = 0;
static unsigned char down = 0;
static unsigned char up = 0;
static unsigned char right = 0;
static unsigned char qUp = 0;
static unsigned char qDown = 0;
static unsigned long activeCount = 0;


static unsigned char zoomIn = 0;
static unsigned char zoomOut = 0;
static unsigned char paused = 0;

static float highestSpeed = 0.0f;

void key_callback(GLFWwindow *restrict const win,
				  const int key,
				  const int scancode,
				  const int action,
				  const int mods)
{
	//action 1 press
	if (action != 1 && action != 0)
		return;
	switch (key) {
		case 91:
		qDown = action; break;
		case 93:
		qUp = action; break;
		case 262:
		right = action; break;
		case 263:
		left = action; break;
		case 264:
		down = action; break;
		case 265:
		up = action; break;
		case 81:
		zoomIn = action; break;
		case 65:
		zoomOut = action; break;
		case 32:
		if (action) {paused = !paused; break;}
	}
}
// 91 93 lbrack rbrack
//262 263 264 265 right left down up

struct point
{
	float x,y,z;
	float r,g,b;
};
typedef struct point point;

void HSVtoRGB( float *r, float *g, float *b,const float hin,const float s,const float v )
{
	auto float f, p, q, t;
	if( s == 0 ) {
		// achromatic (grey)
		*r = *g = *b = v;
		return;
	}
	const float h = hin / 60;			// sector 0 to 5
	const int i = floor( h );
	f = h - i;			// factorial part of h
	p = v * ( 1 - s );
	q = v * ( 1 - s * f );
	t = v * ( 1 - s * ( 1 - f ) );
	switch( i ) {
		case 0:
		*r = v;
		*g = t;
		*b = p;
		break;
		case 1:
		*r = q;
		*g = v;
		*b = p;
		break;
		case 2:
		*r = p;
		*g = v;
		*b = t;
		break;
		case 3:
		*r = p;
		*g = q;
		*b = v;
		break;
		case 4:
		*r = t;
		*g = p;
		*b = v;
		break;
		default:		// case 5:
		*r = v;
		*g = p;
		*b = q;
		break;
	}
}

void glhPerspectivef2(const float fovyInDegrees,
					  const float aspectRatio,
					  const float znear,
					  const float zfar)
{
    auto float ymax, xmax;
    ymax = znear * tanf(fovyInDegrees * M_PI / 360.0);
    xmax = ymax * aspectRatio;
    glFrustum( -xmax, xmax, -ymax, ymax, znear, zfar);
}

float randRange(const float min, const float max)
{
	return (rand()*1.0/RAND_MAX)*(max-min) + min;
}

// returns true if the point should stay alive.
float updatePoint(point *restrict const p)
{
	const auto float x = p->x;
	const auto float y = p->y;
	#ifdef THREEDEE
		const auto float z = p->z;
	#endif

	#ifdef THREEDEE
		const float dx_  = MOTION_SCALE*(dx(x,y,z));
		const float dy_  = MOTION_SCALE*(dy(x,y,z));
		const float dz_  = MOTION_SCALE*(dz(x,y,z));
	#else
		const float dx_  = MOTION_SCALE*(dx(x,y));
		const float dy_  = MOTION_SCALE*(dy(x,y));
	#endif
	p->x += dx_;
	p->y += dy_;
	#ifdef THREEDEE
		p->z += dz_;
		const float vel = sqrt(dx_*dx_ + dy_*dy_ + dz_*dz_);
	#else
		const float vel = sqrt(dx_*dx_ + dy_*dy_);
	#endif

	auto float r,g,b;
	HSVtoRGB(&r,&g,&b, 240 - (vel/highestSpeed)*240, 1.0, 1.0);
	p->r = r;
	p->g = g;
	p->b = b;

	return vel;
}

point getRandPoint(void)
{
	const point p = {randRange(-10.0,10.0),
					randRange(-10.0,10.0),
					#ifdef THREEDEE
					randRange(-10.0,10.0),
					#endif
					0.0,0.0,0.0};
	return p;
}

static point *points;

struct update_thread_info {
	size_t offset;
	size_t amount;
};
typedef struct update_thread_info update_thread_info;

void *update_thread(void *ptr) {
	update_thread_info *info = (update_thread_info *)ptr;
	float highestvel = 0.0;
	for (size_t i = 0; i < info->amount; i++) {
		#ifdef DO_BATCHES
		if ((info->offset + i) < activeCount) {
		#endif
			float x = updatePoint(points + info->offset + i);
			if (x > highestvel) highestvel = x;
		#ifdef DO_BATCHES
		}
		#endif
	}
	float *const result = malloc(sizeof(float));
	*result = highestvel;
	pthread_exit((void *) result);
	return NULL;
}

signed int main(const signed int argc , const char **argv)
{
	if (!glfwInit())
		return -1;
	glfwWindowHint(GLFW_DEPTH_BITS,16);
	glfwWindowHint(GLFW_RESIZABLE, GL_FALSE);
	GLFWwindow *const window = glfwCreateWindow(800,800,"DiffEq Grapher",NULL,NULL);
	if (!window) {
		glfwTerminate();
		return -2;
	}
	glfwMakeContextCurrent(window);
	glfwSwapInterval(1);
	glfwSetKeyCallback(window,&key_callback);
	glfwSetTime(0.0);
	glMatrixMode(GL_PROJECTION);
	glLoadIdentity();
	#ifdef THREEDEE
		//gluPerspective(60,1.0,0.1,100.0);
		glhPerspectivef2(60,1.0,0.1,100.0);
	#else
		glOrtho(-10.0f,10.0f,-10.0f,10.0f,-1.0f,1.0f);
	#endif

	glMatrixMode(GL_MODELVIEW);
	#ifdef THREEDEE
		glEnable(GL_DEPTH_TEST);
	#endif
	glEnable(GL_POINT_SMOOTH);
	glHint(GL_POINT_SMOOTH_HINT, GL_NICEST);

	srand(time(NULL));
	points = malloc(sizeof(point)*POINT_COUNT);
	if (!points) {
		glfwDestroyWindow(window);
		glfwTerminate();
		return -15;
	}
	for (auto unsigned long x = 0; x < POINT_COUNT; x++) {
		#ifndef DO_BATCHES
			auto point p = getRandPoint();
			points[x] = p;
		#else
			auto point p = getRandPoint();
			points[x] = p;
		#endif
	}

	glColor3f(1.0f,1.0f,1.0f);

	#ifdef DO_BATCHES
		auto unsigned long pointForRemoval = 0;
	#else
		activeCount = POINT_COUNT;
	#endif

	#ifdef THREEDEE
		auto float xRot = 0.0f;
		auto float yRot = 0.0f;
	#else
		auto float translateX = 0.0f;
		auto float translateY = 0.0f;
	#endif

	auto float zoom = 1.0f;
	glPointSize(1.0);

	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_COLOR_ARRAY);
	glLineWidth(2.0);
	GLuint boxList = glGenLists(1);
	glNewList(boxList, GL_COMPILE);
	glColor3f(1.0f,1.0f,1.0f);
			glBegin(GL_LINE_STRIP);
			glVertex3f(-10.0f,-10.0f,10.0f);
			glVertex3f(-10.0f,10.0f,10.0f);
			glVertex3f(10.0f,10.0f,10.0f);
			glVertex3f(10.0f,-10.0f,10.0f);
			glVertex3f(-10.0f,-10.0f,10.0f);
			glEnd();

			glBegin(GL_LINE_STRIP);
			glVertex3f(-10.0f,-10.0f,-10.0f);
			glVertex3f(-10.0f,10.0f,-10.0f);
			glVertex3f(10.0f,10.0f,-10.0f);
			glVertex3f(10.0f,-10.0f,-10.0f);
			glVertex3f(-10.0f,-10.0f,-10.0f);
			glEnd();

			glBegin(GL_LINE_STRIP);
			glVertex3f(-10.0f,-10.0f,-10.0f);
			glVertex3f(-10.0f,-10.0f,10.0f);
			glVertex3f(10.0f,-10.0f,10.0f);
			glVertex3f(10.0f,-10.0f,-10.0f);
			glVertex3f(-10.0f,-10.0f,-10.0f);
			glEnd();

			glBegin(GL_LINE_STRIP);
			glVertex3f(-10.0f,10.0f,-10.0f);
			glVertex3f(-10.0f,10.0f,10.0f);
			glVertex3f(10.0f,10.0f,10.0f);
			glVertex3f(10.0f,10.0f,-10.0f);
			glVertex3f(-10.0f,10.0f,-10.0f);
			glEnd();

			glColor3f(1.0f,0.0f,0.0f);
			glBegin(GL_LINES);
			glVertex3f(-9.5f,-10.5f,10.5f);
			glVertex3f(-10.5f,-10.5f,10.5f);
			glEnd();

			glColor3f(0.0f,1.0f,0.0f);
			glBegin(GL_LINES);
			glVertex3f(-10.5f,-9.5f,10.5f);
			glVertex3f(-10.5f,-10.5f,10.5f);
			glEnd();

			glColor3f(0.0f,0.0f,1.0f);
			glBegin(GL_LINES);
			glVertex3f(-10.5f,-10.5f,9.5f);
			glVertex3f(-10.5f,-10.5f,10.5f);
			glEnd();
	glEndList();
	glVertexPointer(3,GL_FLOAT,sizeof(point),points);
	glColorPointer(3,GL_FLOAT,sizeof(point), ((float *)(points)) + 3);

	while (1) {
		//if (glGetError())
		//	break;
		if (!paused) {
			t += TIME_STEP;
		}

		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glLoadIdentity();
		#ifdef THREEDEE
			glTranslatef(0.0f,0.0f,-15.0f);
			glScalef(0.4f,0.4f,0.4f);
			glRotatef(xRot,1.0f,0.0f,0.0f);
			glRotatef(yRot,0.0f,1.0f,0.0f);
		#endif

		glScalef(zoom,zoom,zoom);
		#ifndef THREEDEE
			glTranslatef(translateX,translateY,0.0);
		#endif

		if (zoomOut)
			zoom *= 0.95;
		if (zoomIn)
			zoom *= 1.05;
		#ifdef THREEDEE
			if (left)
				yRot -= 1.0f;
			if (right)
				yRot += 1.0f;
			if (down)
				xRot += 1.0f;
			if (up)
				xRot -= 1.0f;
			if (xRot > 90.0f)
				xRot = 90.0f;
			if (xRot < -90.0f)
				xRot = -90.0f;
		#else
			if (left)
				translateX += 0.3/zoom;
			if (up)
				translateY -= 0.3/zoom;
			if (right)
				translateX -= 0.3/zoom;
			if (down)
				translateY += 0.3/zoom;
		#endif

		//drawbox
		#ifdef THREEDEE
		#ifdef BOX
			glCallList(boxList);
			#endif
		#endif


		#ifdef DO_BATCHES
			for (auto unsigned long i = 0; i < POINTS_PER_BATCH && !paused; i++) {
				const point p = getRandPoint();
				points[pointForRemoval] = p;
				pointForRemoval++;
				if (pointForRemoval >= POINT_COUNT)
					pointForRemoval = 0;
				activeCount++;
				if (activeCount >= POINT_COUNT) {
					activeCount = POINT_COUNT;
				}
			}
		#endif


		update_thread_info infos[THREAD_COUNT];
		if (!paused) {
			pthread_t pids[THREAD_COUNT];
			for (unsigned short i = 0; i < THREAD_COUNT; i++) {
				infos[i].offset = i * (POINT_COUNT / THREAD_COUNT);
				infos[i].amount = POINT_COUNT / THREAD_COUNT;
				pthread_create(pids+i, NULL, &update_thread,(void *) (infos+i));
			}
			float *results[THREAD_COUNT];
			for (unsigned short i = 0; i < THREAD_COUNT; i++) {
				pthread_join(pids[i], (void *)(results+i));
			}
			#ifdef RECALCULATE_COLOR
				highestSpeed = 0;
			#endif
			for (unsigned short i = 0; i < THREAD_COUNT; i++) {
				if (*results[i] > highestSpeed) {
					highestSpeed = *results[i];
					free(results[i]);
				}
			}
		}

		#ifdef THREEDEE
		#ifndef LINES

		glDrawArrays(GL_POINTS,0,activeCount);
		#endif
		#endif

		if (qUp) q += Q_STEP;
		if (qDown) q -= Q_STEP;

		glfwSwapBuffers(window);
		glfwPollEvents();
		if (glfwWindowShouldClose(window))
			break;
	}
	glDeleteLists(boxList,1);
	glfwDestroyWindow(window);
	glfwTerminate();
	return 0;
}
