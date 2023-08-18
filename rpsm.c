/*
 *                      ***** TA RPS Multicast Example *****
 *
 *                      Transparent Architecture
 *
 *   Copyright (c) 2012 General Energy Technologies. All rights reserved.
 *
 */

#ifdef WITH_PYTHON
#include <Python.h>
#endif
#include <ta.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ------------------------------------------------------- */
TA_STRUCT(Output,
typedef struct {
	TaChar output;
	TaChar input;
	TaShort result;
	TaLong totals;
	TaLong dc_counter;
	TaLong myc_counter;
} Output)
TA_STRUCT(Control,
typedef struct {
	TaChar command;
} Control)
TA_STRUCT(OutputSeq,
typedef struct {
	Output *outputs;
} OutputSeq)

/* ------------------------------------------------------- */
#define MAX_STATIONS	40
TaMessageQos g_mqos;
int initHostScript(TaStation *station);

void MakeYourChoice(Output *outp, int dst_sid);
void DoCalculate(Output *outp, int dst_sid);
void DoControl(TaTable *table, void *par);
void OnMessage(const void *data, TaULong msg_type,
        const struct TaTable *t);
int RPSmUpdate(TaArea *area, TaLongLong t_updating, void *client_data);

/* ------------------------------------------------------- */
OutputSeq g_outputSeq;
Control g_control;
TaTable g_tab_control, g_tab_outputs;

TaBool initialized;
int g_enable_emgr;
const int MAX_N_MOVES = 1000;
#define START_DELAY 10000000

/* ------------------------------------------------------- */
int g_played[MAX_STATIONS];
TaLongLong g_played_time[MAX_STATIONS];
#ifdef WITH_PYTHON
PyObject *g_moves[MAX_STATIONS];
#endif

int totals;
float done_pct;

int usage()
{
	printf(
"usage: rpsm -n number ?-option value? station-0 .. station-N\n"
"where:\n"
"'station-1 .. station-N' is address: hostname:script[:port], max amount=%d\n"
"'-n number' is current station number [0-%d]\n"
"'-e enable' enables protocol by bits:\n"
"\t1 - UDP multicast, 131072 - Shared Memory Multicast\n"
"'-c cycle' sets new runtime cycle in mcs [1000-10000000]\n"
"'-f frame' sets initial frame to [1-10] or current station if negative\n"
"'-r 1' sets redundand sending on\n"
"'-a area' sets area number [4,1000000]\n"
"'-p phase_correct' sets phase_correct interval [0,..], def:60000000\n"
"'-m max_poll_fails' sets max_poll_fails def:10\n"
"'-t trace' enables tracing by bits:\n"
"\t1 - RUNTIME, 2 - TRANSPORT, 4 - POLLING, 8 - MESSAGE, 16 - MESSAGE DATA\n",
	MAX_STATIONS, MAX_STATIONS-1);
	return 1;
}

/* ------------------------------------------------------- */
int main(int argc, char **argv)
{
	TaArea area;
	TaAreaQos aqos;
	TaLong aid = 5;
	TaStation stations[MAX_STATIONS];
	TaStationQos sqos;
	TaChannelQos cqos;
	int i, my_sid_number, stations_amount;
	int sqos_first_port;

	if (argc < 3)
		return usage();

#ifdef WITH_PYTHON
	Py_SetProgramName(argv[0]);
	Py_Initialize();
#endif

	taInitialize();
	taDefaultAreaQos(&aqos);
	aqos.tp_cycle = 60000;
	aqos.messages_array_size = 1000;
	taInitArea(aid, &aqos, "rps", &area);
	taReallocStations(MAX_STATIONS, &area);
	taDefaultStationQos(&sqos);
	sqos_first_port = sqos.first_port;
	taDefaultChannelQos(&cqos);
	cqos.max_send_polls = 10;
//cqos.tp_receiving_timeout=10000000;
	taDefaultMessageQos(&g_mqos);
	g_mqos.tp_timeout = 60000000;
	g_mqos.tp_send_timeout = 30000000;
	//g_mqos.n_send_attempts = 5;

	//sqos.tp_discovery_timeout = 30000000;
	//sqos.tp_ping_timeout = 20000000;
	my_sid_number = -1;
	stations_amount = 0;
	g_enable_emgr = 3;
	for (i = 1; i < argc && stations_amount < MAX_STATIONS; i++) {
		if (!strcmp("-n", argv[i])) {
			if (argv[i+1])
				my_sid_number = atoi(argv[i+1]);
			++i;
		} else if (!strcmp("-t", argv[i])) {
			if (argv[i+1])
				taTrace(ta_trace_level = atoi(argv[i+1]), NULL);
			++i;
		} else if (!strcmp("-e", argv[i])) {
			if (argv[i+1]) {
				aqos.enable_mask = sqos.enable_mask =
					sqos.ping_mask = atoi(argv[i+1]);
				if (aqos.enable_mask != TA_ENABLE_UDP_MCAST
				&&  aqos.enable_mask != TA_ENABLE_SHM_MCAST)
					return usage();
				taInitArea(aid, &aqos, "rps", &area);
				taReallocStations(MAX_STATIONS, &area);
			}
			++i;
		} else if (!strcmp("-p", argv[i])) {
			if (argv[i+1])
				sqos.ping_mask = atoi(argv[i+1]);
			++i;
		} else if (!strcmp("-c", argv[i])) {
			if (argv[i+1])
				area.qos.tp_cycle = atoi(argv[i+1]);
			if (area.qos.tp_cycle < 1000)
				area.qos.tp_cycle = 1000;
			if (area.qos.tp_cycle > 10000000)
				area.qos.tp_cycle = 10000000;
			++i;
		} else if (!strcmp("-f", argv[i])) {
			if (argv[i+1])
				area.qos.justify_frame = atoi(argv[i+1]);
			++i;
		} else if (!strcmp("-r", argv[i])) {
			if (argv[i+1])
				g_mqos.is_redundant = (TaBool)atoi(argv[i+1]);
			++i;
		} else if (!strcmp("-a", argv[i])) {
			if (argv[i+1])
				area.aid = aid = (TaLong)atoi(argv[i+1]);
			++i;
		} else if (!strcmp("-p", argv[i])) {
			if (argv[i+1])
				area.qos.tp_phase_correct = atoi(argv[i+1]);
			++i;
		} else if (!strcmp("-m", argv[i])) {
			if (argv[i+1])
				area.qos.max_poll_fails = atoi(argv[i+1]);
			++i;
		} else if (!strcmp("-x", argv[i])) {
			if (argv[i+1])
				g_enable_emgr = atoi(argv[i+1]);
			++i;
		} else {
			char s_name[128], *p;
			if (*argv[i] == '*') {
				strcpy(sqos.hostname, argv[i]+1);
				if (my_sid_number < 0)
					my_sid_number = stations_amount;
			} else
				strcpy(sqos.hostname, argv[i]);
			p = strchr(sqos.hostname, ':');
			if (!p)
				return usage();
			*p = 0;
			strcpy(s_name, p+1);
			p = strchr(s_name, ':');
			if (p) {
				*p = 0;
				sqos.first_port = atoi(p+1);
			} else
				sqos.first_port = sqos_first_port;
			taInitStation(stations_amount, &sqos, &cqos, s_name, 
				&area, &stations[stations_amount]);
			++stations_amount;
		}
	}

	if (my_sid_number < 0 || my_sid_number >= stations_amount)
		return usage();
	taSetStationHostBySid(my_sid_number, &area);
	taInitStationsData(&area);
#ifdef WITH_PYTHON
	initHostScript(area.host_station);
#endif

	i = taParseToRTTI(g_Output, "Tests.IRPS", &g_rtti_Output, 0);
	i |= taParseToRTTI(g_Control, "Tests.IRPS", &g_rtti_Control, 0);
	i |= taParseToRTTI(g_OutputSeq, "Tests.IRPS", &g_rtti_OutputSeq, 0);

	taNewTable(&area, &g_tab_control);
	g_tab_control.mqos = g_mqos;
	taAttachTable(&g_tab_control, &g_rtti_Control, 0, 0, 1, OnMessage,
		NULL, TA_N_SIDS_DEFAULT);
	taNewTable(&area, &g_tab_outputs);
	g_tab_outputs.mqos = g_mqos;
	taAttachTable(&g_tab_outputs, &g_rtti_OutputSeq, 1, 1, 1, OnMessage,
		NULL, TA_N_SIDS_DEFAULT);
	g_outputSeq.outputs = (Output *)
		taAllocWithRTTI(sizeof(Output), stations_amount);

	if (my_sid_number == 0) {
		g_control.command = 'I';
		taWaitTable(&g_tab_control, taGetTime()+START_DELAY);
		taWriteTable(&g_tab_control, &g_control, 
			TA_ADDR_ALL, _False);
		taCallTable(&g_tab_control, DoControl, &my_sid_number);
	} else {
		taWaitTable(&g_tab_control, TA_WAIT_ANY_MESSAGE);
		taCallTable(&g_tab_control, DoControl, &my_sid_number);
	}
	totals = 0;
	done_pct = 0;
	initialized = _False;

	area.on_update = RPSmUpdate;
	taRun(&area, &my_sid_number);

#ifdef WITH_PYTHON
	Py_Finalize();
#endif
	taCloseStationsData(&area);
	return 0;
}

/* ------------------------------------------------------- */
void DoPrintResults()
{
	TaArea *area;
	int i, ndone, nerr, my_sid;
	float f;
	area = g_tab_outputs.area;
	my_sid = area->host_station->sid;

	// Is Complete
	for (i = ndone = nerr = 0; i < area->n_stations; i++) {
		if (g_played[i] == MAX_N_MOVES)
			++ndone;
		else if (g_played[i] != 0)
			++nerr;
	}
	if (ndone+nerr < area->n_stations-1) {
		printf("...%s:%d ndone:%d nerr:%d\n", area->host_station->name,
			(int) my_sid, ndone, nerr);
		return;
	}
	for (i = ndone = 0; i < area->n_stations; i++) {
		if (i == my_sid)
			continue;
		f = 50+50.*g_outputSeq.outputs[i].totals/
			(g_outputSeq.outputs[i].dc_counter-1);
		printf("%s%s:%d=vs=%s:%d played:%d scope:%g%% totals:%d "
		"dc/myc:%d/%d\n",
			(g_outputSeq.outputs[i].dc_counter == 
				MAX_N_MOVES ? "" : "~"),
			area->host_station->name, (int) my_sid,
			area->stations[i]->name, i,
			g_played[i], f, (int)g_outputSeq.outputs[i].totals, 
			(int)g_outputSeq.outputs[i].dc_counter, 
			(int)g_outputSeq.outputs[i].myc_counter);
		if(g_outputSeq.outputs[i].dc_counter == MAX_N_MOVES)
			++ndone;
	}
	printf("===%s:%d ndone:%d\n", area->host_station->name, 
		(int) my_sid, ndone);
	area->t_expired = taGetTime() + 1000000;
}

/* ------------------------------------------------------- */
void Evaluate(int sid, TaChar input, TaChar *output)
{
	TaStation *station;
	station = g_tab_outputs.area->stations[sid];
	if (!strcmp(station->name, "r"))
		*output = 'R';
	else if (!strcmp(station->name, "p"))
		*output = 'P';
	else if (!strcmp(station->name, "s"))
		*output = 'S';
	else if (!strcmp(station->name, "rand")) {
		int ir = rand();
		if (ir <= RAND_MAX/3)
			*output = 'R';
		else if (ir <= RAND_MAX/3*2)
			*output = 'P';
		else
			*output = 'S';
	} else if (!strcmp(station->name, "brand")) {
		int bias = 0, ir;
		if (input == 'R')
			bias = 0;
		else if (input == 'P')
			bias = RAND_MAX/2;
		else
			bias = -RAND_MAX/2;
		ir = rand()/2+bias;
		if (ir <= RAND_MAX/3)
			*output = 'R';
		else if (ir <= RAND_MAX/3*2)
			*output = 'P';
		else
			*output = 'S';
	} else {
#ifdef WITH_PYTHON
		PyObject *pArgs, *pValue;
		char buf[2], *s;
		buf[0] = input;
		buf[1] = 0;
		pArgs = PyTuple_New(1);
		pValue = PyString_FromString(buf);
		PyTuple_SetItem(pArgs, 0, pValue);
		pValue = PyObject_CallObject(g_moves[sid], pArgs);
		if (pValue) {
			s = PyString_AsString(pValue);
			*output = *s;
			Py_DECREF(pValue);
		} else {
			printf("err input:%c\n", 
				(input=='\0' ? ' ' : input));
			*output = 0;
			PyErr_Print();
		}
		Py_DECREF(pArgs);
#else
		*output = '\0';
#endif
	}
}

/* ------------------------------------------------------- */
void MakeYourChoice(Output *outp, int dst_sid)
{
	Output *outhost;
        outhost = &g_outputSeq.outputs[dst_sid];
//printf("=MakeYourChoice %s src:%d dst:%d myc:%d/%d dc:%d/%d\n",
//table->area->host_station->name, table->src_oid, table->dst_oid,
//outp->myc_counter, outhost->myc_counter, outp->dc_counter, outhost->dc_counter);
	Evaluate(dst_sid, outhost->input, &outhost->output);
	++outhost->myc_counter;
}

/* ------------------------------------------------------- */
void DoCalculate(Output *outp, int dst_sid)
{
	Output *outhost;
	int i, all;
	outhost = &g_outputSeq.outputs[dst_sid];

	if (!outhost->output || !outp->output)
		outhost->result = 0;
	else if (outhost->output == outp->output)
		outhost->result = 0;
	else if (!strchr("RPS", outhost->output))
		outhost->result = -1;
	else if (!strchr("RPS", outp->output))
		outhost->result = 1;
	else if (outhost->output == 'R') {
		if (outp->output == 'S')
			outhost->result = 1;
		else
			outhost->result = -1;
	} else if (outhost->output == 'S') {
		if (outp->output == 'R')
			outhost->result = -1;
		else 
			outhost->result = 1;
	} else {
		if (outp->output == 'R')
			outhost->result = 1;
		else
			outhost->result = -1;
	}
	outhost->totals += outhost->result;
	outhost->input = outp->output;
	++outhost->dc_counter;
	if (outhost->dc_counter >= MAX_N_MOVES) {
		g_played[dst_sid] = outhost->dc_counter;
		DoPrintResults();
	}

	all = 0; totals = 0;
	for (i = 0; i < g_tab_outputs.area->n_stations; i++) {
		if (i == g_tab_outputs.area->host_station->sid)
			continue;
		all += g_outputSeq.outputs[i].dc_counter;
		totals += g_outputSeq.outputs[i].totals;
	}
	done_pct = 100.0*all/(g_tab_outputs.area->n_stations-1)/MAX_N_MOVES;
	if (ta_trace_level & 128 || ((ta_trace_level & 64) && done_pct >= 100))
		printf("%s totals:%d done_pct:%g\n", 
			g_tab_outputs.area->host_station->name, 
				totals, done_pct);
}

/* ------------------------------------------------------- */
void DoControl(TaTable *table, void *par)
{
	Control *node;
	int i, *my_sid_number = (int *) par;
//	printf("DoControl\n");
	if (*my_sid_number == 0)
		node = &g_control;
	else
		node = (Control *) table->recent_data;
	if (ta_trace_level & 64) {
		printf("%d command %c\n", *my_sid_number, node->command);
		taDumpStationLinks(table->area);
	}
	if (node->command == 'I') {
		initialized = _True;
		for (i = 0; i < table->area->n_stations; i++) {
			if (i == *my_sid_number)
				continue;
			g_outputSeq.outputs[i].input = '\0';
			MakeYourChoice(&g_tab_outputs, i);
		}
	}

}

/* ------------------------------------------------------- */
void OnMessage(const void *data, TaULong msg_type,
        const struct TaTable *t)
{
	if (msg_type == g_tab_outputs.msg_type) {
		OutputSeq *seq = (OutputSeq *) data;
		int dst_sid = t->message.src_sid;
		Output *outp;
		if (dst_sid < 0 || dst_sid >= t->area->n_stations)
			return;
		if (t->area->n_stations != taLengthWithRTTI(seq->outputs))
			return;
		outp = &seq->outputs[t->area->host_station->sid];
		/*printf("msg dst:%d dc:%d/%d myc:%d/%d\n", dst_sid, 
			outp->dc_counter, 
			g_outputSeq.outputs[dst_sid].dc_counter,
			outp->myc_counter, 
			g_outputSeq.outputs[dst_sid].myc_counter);
		*/
		if (g_outputSeq.outputs[dst_sid].dc_counter < MAX_N_MOVES) {
			if (outp->dc_counter == 
			g_outputSeq.outputs[dst_sid].dc_counter &&
			g_outputSeq.outputs[dst_sid].myc_counter ==
			g_outputSeq.outputs[dst_sid].dc_counter)
				MakeYourChoice(outp, dst_sid);
			else if (outp->myc_counter ==
			g_outputSeq.outputs[dst_sid].myc_counter &&
			g_outputSeq.outputs[dst_sid].myc_counter ==
			g_outputSeq.outputs[dst_sid].dc_counter+1)
				DoCalculate(outp, dst_sid);
		}
		g_played_time[t->message.src_sid] = t->area->t_crtu;
	} else if (msg_type == taErrorDataMsgType()) {
                TaErrorData *edp = (TaErrorData *) data;
                printf("*** error:%d %s t:%u src:%d dst:%d\n", 
			edp->errcode, t->area->host_station->name,
			(int)(t->area->t_crtu/1000000), 
			t->src_oid, t->dst_oid);
		// finish on error
		if (t == &g_tab_control) {
			printf("===%s src:%d finished by timeout\n", 
				t->area->host_station->name, 
				t->area->host_station->sid);
			t->area->t_expired = taGetTime() + 1000000;
		} else {
			g_played[t->dst_oid] = edp->errcode;
			DoPrintResults();
		}
        }
}

/* ------------------------------------------------------- */
int RPSmUpdate(TaArea *area, TaLongLong t_updating, void *client_data)
{
	if (!initialized)
		return 0;
	taWriteTable(&g_tab_outputs, &g_outputSeq, TA_ADDR_ALL, _False);
	return 1;
}

/* ------------------------------------------------------- */
#ifdef WITH_PYTHON
int initHostScript(TaStation *station)
{
	PyObject *pName, *pModule, *pFunc;
	if (	!strcmp("r", station->name)
	||	!strcmp("p", station->name)
	||	!strcmp("s", station->name)
	||	!strcmp("rand", station->name)
	||	!strcmp("brand", station->name))
			return 0;
	pName = PyString_FromString("rpsupport");
	pModule = PyImport_Import(pName);
	Py_DECREF(pName);
	pFunc = PyObject_GetAttrString(pModule, "load_bots");
//	printf("pModule=%x,pFunc=%x ver:%s\n", pModule, pFunc, Py_GetVersion());
	if (pFunc && PyCallable_Check(pFunc)) {
		PyObject *pArgs, *pValue;
		char buffer[64];
		int i;
		sprintf(buffer, "py/%s.py", station->name);
		pArgs = PyTuple_New(1);
		pValue = PyString_FromString(buffer);
		PyTuple_SetItem(pArgs, 0, pValue);
		// one instance for each partnew
		for (i = 0; i < station->area->n_stations; i++) {
			TaStation *st = (TaStation *)station->area->stations[i];
			if (!st)
				continue;
			if (st == station)
				g_moves[i] = NULL;
			else {
				pValue = PyObject_CallObject(pFunc, pArgs);
				g_moves[i] = PyObject_GetAttrString(pValue, 
					"get_move");
			}
		}

		Py_DECREF(pValue);
		Py_DECREF(pArgs);
		Py_DECREF(pFunc);
	}

	return 0;
}
#endif

