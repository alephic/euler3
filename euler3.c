/*
 * EULER3 Differential Equation Grapher
 * Luke Nelson and Kaj Bostrom
 */

#include <GLFW/glfw3.h>
#include <math.h>
#include <stdlib.h>
#include <stdio.h>
#include <sys/stat.h>
#include <sys/types.h>
#include <time.h>
#include <unistd.h>
#include <assert.h>
#include <pthread.h>

#define THREAD_COUNT 2
#define LAG 1
#define RECALCULATE_COLOR
//#define LINES
#define TIME_STEP 1.0
#define MOTION_SCALE 0.02
#define Q_STEP 0.02
#define BOX
#define BOX_SIZE 40.0f

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

struct vec3f
{
	float x,y,z;
};
typedef struct vec3f vec3f;

static point *points;
static size_t point_count;
static size_t num_time_steps;

static vec3f *input_vecs;

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

float updatePoint(size_t point_index)
{
	vec3f origin = input_vecs[((int) floor(t))*point_count + point_index];
	vec3f dest = input_vecs[(((int) floor(t)) + 1)*point_count + point_index];
	const float sub_step = t - floor(t);
	const auto float dx_ = dest.x - origin.x;
	const auto float dy_ = dest.y - origin.y;
	const auto float dz_ = dest.z - origin.z;
	point *p = points+point_index;
	p->x = origin.x + (dest.x - origin.x)*sub_step;
	p->y = origin.y + (dest.y - origin.y)*sub_step;
	p->z = origin.z + (dest.z - origin.z)*sub_step;

	const float vel = sqrt(dx_*dx_ + dy_*dy_ + dz_*dz_);

	//auto float r,g,b;
	//HSVtoRGB(&r,&g,&b, 240 - (vel/highestSpeed)*240, 1.0, 1.0);
	p->r = 1.0f;
	p->g = 1.0f;
	p->b = 1.0f;

	return vel;
}

struct update_thread_info {
	size_t offset;
	size_t amount;
};
typedef struct update_thread_info update_thread_info;

void *update_thread(void *ptr) {
	update_thread_info *info = (update_thread_info *)ptr;
	float highestvel = 0.0;
	for (size_t i = 0; i < info->amount; i++) {
		float x = updatePoint(info->offset + i);
		if (x > highestvel) highestvel = x;
	}
	float *const result = malloc(sizeof(float));
	*result = highestvel;
	pthread_exit((void *) result);
	return NULL;
}

signed int main(const signed int argc , const char **argv)
{
	if (argc != 2) {
		printf("Usage: ./euler3 [input file]\n");
		return -5;
	}

	FILE *inpFile;
	char *inpBuf;
	size_t inpCols = 1;

	inpFile = fopen(argv[1], "r");
	if (!inpFile) {
		fprintf(stderr, "Unable to open input file\n");
		return -6;
	}

	struct stat st;
	if (fstat(fileno(inpFile), &st)) {
		fclose(inpFile);
		return -6;
	}

	inpBuf = (char *) malloc(st.st_size);
	if (!inpBuf) {
		fclose(inpFile);
		return -7;
	}

	if (fread((void *) inpBuf, 1, st.st_size, inpFile) != st.st_size) {
		fclose(inpFile);
		return -8;
	}
	fclose(inpFile);

	num_time_steps = 0;
	char *curr = inpBuf;

	for (size_t i = 0; i < st.st_size; ++i) {
		if (*curr == '\n') {
			++num_time_steps;
		} else if (num_time_steps == 0 && *curr == ' ') {
			++inpCols;
		}
		++curr;
	}

	point_count = inpCols / 3;

	printf("Displaying %lu points for %lu timesteps\n", point_count, num_time_steps);

	input_vecs = malloc(sizeof(vec3f) * point_count * num_time_steps);
	if (!input_vecs) {
		return -7;
	}

	curr = inpBuf;
	for (size_t i = 0; i < point_count * num_time_steps; ++i) {
		input_vecs[i].x = strtof(curr, &curr);
		input_vecs[i].y = strtof(curr, &curr);
		input_vecs[i].z = strtof(curr, &curr);
	}

	free(inpBuf);


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
	//gluPerspective(60,1.0,0.1,100.0);
	glhPerspectivef2(60,1.0,0.1,100.0);

	glMatrixMode(GL_MODELVIEW);
	glEnable(GL_DEPTH_TEST);
	glEnable(GL_POINT_SMOOTH);
	glHint(GL_POINT_SMOOTH_HINT, GL_NICEST);

	points = malloc(sizeof(point)*point_count);

	if (!points) {
		glfwDestroyWindow(window);
		glfwTerminate();
		return -15;
	}
	for (auto unsigned long x = 0; x < point_count; x++) {
		points[x].x = input_vecs[x].x;
		points[x].y = input_vecs[x].y;
		points[x].z = input_vecs[x].z;
		points[x].r = 0.0f;
		points[x].g = 0.0f;
		points[x].b = 0.0f;
	}

	glColor3f(1.0f,1.0f,1.0f);

	activeCount = point_count;

	auto float xRot = 0.0f;
	auto float yRot = 0.0f;

	auto float zoom = 10.0f/BOX_SIZE;
	glPointSize(10.0);

	glEnableClientState(GL_VERTEX_ARRAY);
	glEnableClientState(GL_COLOR_ARRAY);
	glLineWidth(2.0);
	GLuint boxList = glGenLists(1);
	glNewList(boxList, GL_COMPILE);
	glColor3f(1.0f,1.0f,1.0f);
			glBegin(GL_LINE_STRIP);
			glVertex3f(-BOX_SIZE,-BOX_SIZE,BOX_SIZE);
			glVertex3f(-BOX_SIZE,BOX_SIZE,BOX_SIZE);
			glVertex3f(BOX_SIZE,BOX_SIZE,BOX_SIZE);
			glVertex3f(BOX_SIZE,-BOX_SIZE,BOX_SIZE);
			glVertex3f(-BOX_SIZE,-BOX_SIZE,BOX_SIZE);
			glEnd();

			glBegin(GL_LINE_STRIP);
			glVertex3f(-BOX_SIZE,-BOX_SIZE,-BOX_SIZE);
			glVertex3f(-BOX_SIZE,BOX_SIZE,-BOX_SIZE);
			glVertex3f(BOX_SIZE,BOX_SIZE,-BOX_SIZE);
			glVertex3f(BOX_SIZE,-BOX_SIZE,-BOX_SIZE);
			glVertex3f(-BOX_SIZE,-BOX_SIZE,-BOX_SIZE);
			glEnd();

			glBegin(GL_LINE_STRIP);
			glVertex3f(-BOX_SIZE,-BOX_SIZE,-BOX_SIZE);
			glVertex3f(-BOX_SIZE,-BOX_SIZE,BOX_SIZE);
			glVertex3f(BOX_SIZE,-BOX_SIZE,BOX_SIZE);
			glVertex3f(BOX_SIZE,-BOX_SIZE,-BOX_SIZE);
			glVertex3f(-BOX_SIZE,-BOX_SIZE,-BOX_SIZE);
			glEnd();

			glBegin(GL_LINE_STRIP);
			glVertex3f(-BOX_SIZE,BOX_SIZE,-BOX_SIZE);
			glVertex3f(-BOX_SIZE,BOX_SIZE,BOX_SIZE);
			glVertex3f(BOX_SIZE,BOX_SIZE,BOX_SIZE);
			glVertex3f(BOX_SIZE,BOX_SIZE,-BOX_SIZE);
			glVertex3f(-BOX_SIZE,BOX_SIZE,-BOX_SIZE);
			glEnd();

			glColor3f(1.0f,0.0f,0.0f);
			glBegin(GL_LINES);
			glVertex3f(-BOX_SIZE + (BOX_SIZE/ 5.0f),-BOX_SIZE - (BOX_SIZE/20.0f),BOX_SIZE + (BOX_SIZE/20.0f));
			glVertex3f(-BOX_SIZE - (BOX_SIZE/20.0f),-BOX_SIZE - (BOX_SIZE/20.0f),BOX_SIZE + (BOX_SIZE/20.0f));
			glEnd();

			glColor3f(0.0f,1.0f,0.0f);
			glBegin(GL_LINES);
			glVertex3f(-BOX_SIZE - (BOX_SIZE/20.0f),-BOX_SIZE + (BOX_SIZE/ 5.0f),BOX_SIZE + (BOX_SIZE/20.0f));
			glVertex3f(-BOX_SIZE - (BOX_SIZE/20.0f),-BOX_SIZE - (BOX_SIZE/20.0f),BOX_SIZE + (BOX_SIZE/20.0f));
			glEnd();

			glColor3f(0.0f,0.0f,1.0f);
			glBegin(GL_LINES);
			glVertex3f(-BOX_SIZE - (BOX_SIZE/20.0f),-BOX_SIZE - (BOX_SIZE/20.0f),BOX_SIZE - (BOX_SIZE/ 5.0f));
			glVertex3f(-BOX_SIZE - (BOX_SIZE/20.0f),-BOX_SIZE - (BOX_SIZE/20.0f),BOX_SIZE + (BOX_SIZE/20.0f));
			glEnd();
	glEndList();
	glVertexPointer(3,GL_FLOAT,sizeof(point),points);
	glColorPointer(3,GL_FLOAT,sizeof(point), ((float *)(points)) + 3);

	while (1) {
		//if (glGetError())
		//	break;
		if (!paused) {
			q = fmaxf(0.0f, q);
			t += TIME_STEP + q;
			if (((int) floor(t)) >= num_time_steps - 1) {
				t = 0.0f;
			}
		}

		glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT);
		glLoadIdentity();
		glTranslatef(0.0f,0.0f,-15.0f);
		glScalef(0.4f,0.4f,0.4f);
		glRotatef(xRot,1.0f,0.0f,0.0f);
		glRotatef(yRot,0.0f,1.0f,0.0f);

		glScalef(zoom,zoom,zoom);

		if (zoomOut)
			zoom *= 0.95;
		if (zoomIn)
			zoom *= 1.05;
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

		//drawbox
		#ifdef BOX
			glCallList(boxList);
		#endif

		update_thread_info infos[THREAD_COUNT];
		if (!paused) {
			pthread_t pids[THREAD_COUNT];
			for (unsigned short i = 0; i < THREAD_COUNT; i++) {
				infos[i].offset = i * (point_count / THREAD_COUNT);
				infos[i].amount = point_count / THREAD_COUNT;
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

		#ifndef LINES
		glDrawArrays(GL_POINTS,0,activeCount);
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
	free(input_vecs);
	free(points);
	return 0;
}
