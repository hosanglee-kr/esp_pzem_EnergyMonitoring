/// ESPEM - ESP Energy monitor
//  A code for ESP32 based boards to interface with PeaceFair PZEM PowerMeters
//  It can poll/collect PowerMeter data and provide it for futher processing in text/json format

//  (c) Emil Muratov 2018-2022  https://github.com/vortigont/espem


#pragma once
// #include "main.h"
#include "pzem_edl.hpp"
#include "timeseries.hpp"

// Tasker object from EmbUI
#include "ts.h"

// Defaults
#ifndef DEFAULT_WS_UPD_RATE
	#define DEFAULT_WS_UPD_RATE 2  // ws clients update rate, sec
#endif

#define PZEM_ID		   1
#define PORT_1_ID	   1

#define TS_T1_CNT	   900	 // default Tier 1 TimeSeries count
#define TS_T1_INTERVAL 1	 // default Tier 1 TimeSeries interval (1 sec)
#define TS_T2_CNT	   1000	 // default Tier 2 TimeSeries count
#define TS_T2_INTERVAL 15	 // default Tier 2 TimeSeries interval (15 sec)
#define TS_T3_CNT	   1000	 // default Tier 3 TimeSeries count
#define TS_T3_INTERVAL 300	 // default Tier 3 TimeSeries interval (5 min)

// Metrics collector state
enum class mcstate_t {
	MC_DISABLE = 0,
	MC_RUN,
	MC_PAUSE
};

// TaskScheduler - Let the runner object be a global, single instance shared between object files.
extern Scheduler ts;


/////////

// #include "espem.h"
#include "EmbUI.h"	// EmbUI framework

#define 		MAX_FREE_MEM_BLK		ESP.getMaxAllocHeap()
#define 		PUB_JSSIZE			1024
// sprintf template for json sampling data
#define 		JSON_SMPL_LEN			85	 	// {"t":1615496537000,"U":229.50,"I":1.47,"P":1216,"W":5811338,"hz":50.0,"pF":0.64},



#if  defined(G_B00_PZEM_MODEL_PZEM003)
    static const char	PGsmpljsontpl[] PROGMEM 	= "{\"t\":%u000,\"U\":%.2f,\"I\":%.2f,\"P\":%.0f,\"W\":%.0f},";
    static const char	PGdatajsontpl[] PROGMEM 	= "{\"age\":%llu,\"U\":%.1f,\"I\":%.2f,\"P\":%.0f,\"W\":%.0f}";
#elif defined(G_B00_PZEM_MODEL_PZEM004V3)
    static const char	PGsmpljsontpl[] PROGMEM 	= "{\"t\":%u000,\"U\":%.2f,\"I\":%.2f,\"P\":%.0f,\"W\":%.0f,\"hz\":%.1f,\"pF\":%.2f},";
    static const char	PGdatajsontpl[] PROGMEM 	= "{\"age\":%llu,\"U\":%.1f,\"I\":%.2f,\"P\":%.0f,\"W\":%.0f,\"hz\":%.1f,\"pF\":%.2f}";
#endif

// HTTP responce messages
static const char       PGsmpld[]			= "Metrics collector disabled";
static const char       PGdre[]				= "Data read error";
static const char       PGacao[]		        = "Access-Control-Allow-Origin";
static const char*      PGmimetxt			= "text/plain";
// static const char* PGmimehtml = "text/html; charset=utf-8";

/////////////////
void block_menu(Interface *interf);

 // @brief callback method to print debug data on PZEM RX
 // * @param id
 // * @param m
void msgdebug(uint8_t id, const RX_msg *m);

using namespace pzmbus;	 // use general pzem abstractions

///////////////////

class FrameSendMQTTRaw : public FrameSendMQTT {
   public:
	FrameSendMQTTRaw(EmbUI *emb)
		: FrameSendMQTT(emb) {
	}
	void send(const JsonVariantConst &data) override {
		if (data[P_pkg] == C_espem) {
			_eu->publish(C_mqtt_pzem_jmetrics, data[P_block]);
		}
	};
};


////////////////
template <class T>
class DataStorage : public TSContainer<T> {
	std::vector<uint8_t> tsids;

	// energy offset
	int32_t	nrg_offset{0};

   public:
	
	// @brief setup TimeSeries Container based on saved params in EmbUI config
	void reset();

	
	// @brief Set the Energy offset value
	// tis will offset energy value replies from PZEM
	// i.e. to match some other counter, etc...
	//  @param offset
	
	void setEnergyOffset(int32_t offset) {
		nrg_offset = offset;
	}

	// @brief Get the Energy offset value
	 // @return float
	int32_t getEnergyOffset() {
		return nrg_offset;
	}

	void wsamples(AsyncWebServerRequest *request);
};

template <class T>
void DataStorage<T>::reset() {
	this->purge();
	tsids.clear();

	uint8_t a;
	a = this->addTS( 
		  embui.paramVariant(V_TS_T1_CNT)
		, time(nullptr)
		, embui.paramVariant(V_TS_T1_INT)
		, "Tier 1"
		, 1
		);
	tsids.push_back(a);
	// LOG(printf, "Add TS: %d\n", a);

	a = this->addTS(
		  embui.paramVariant(V_TS_T2_CNT)
		, time(nullptr)
		, embui.paramVariant(V_TS_T2_INT)
		, "Tier 2"
		, 2
		);
	tsids.push_back(a);
	// LOG(printf, "Add TS: %d\n", a);

	a = this->addTS(
		  embui.paramVariant(V_TS_T3_CNT)
		, time(nullptr)
		, embui.paramVariant(V_TS_T3_INT)
		, "Tier 3"
		, 3
		);
	tsids.push_back(a);
	// LOG(printf, "Add TS: %d\n", a);

	LOG(println, "Setup TimeSeries DB:");
	LOG_CALL(
		for (auto i : tsids) {
			auto t = this->getTS(i);
			if (t) {
				LOG(printf, "%s: size:%d, interval:%u, mem:%u\n"
					, t->getDescr()
					, t->capacity
					, t->getInterval()
					, t->capacity * sizeof(<T>)
				);
			}
		})

	LOG(printf, "SRAM: heap %u, free %u\n", ESP.getHeapSize(), ESP.getFreeHeap());
	LOG(printf, "SPI-RAM: size %u, free %u\n", ESP.getPsramSize(), ESP.getFreePsram());
}

template <>
void DataStorage<pz004::metrics>::reset() {
	this->purge();
	tsids.clear();

	uint8_t a;
	a = this->addTS( 
		  embui.paramVariant(V_TS_T1_CNT)
		, time(nullptr)
		, embui.paramVariant(V_TS_T1_INT)
		, "Tier 1"
		, 1
		);
	tsids.push_back(a);
	// LOG(printf, "Add TS: %d\n", a);

	a = this->addTS(
		  embui.paramVariant(V_TS_T2_CNT)
		, time(nullptr)
		, embui.paramVariant(V_TS_T2_INT)
		, "Tier 2"
		, 2
		);
	tsids.push_back(a);
	// LOG(printf, "Add TS: %d\n", a);

	a = this->addTS(
		  embui.paramVariant(V_TS_T3_CNT)
		, time(nullptr)
		, embui.paramVariant(V_TS_T3_INT)
		, "Tier 3"
		, 3
		);
	tsids.push_back(a);
	// LOG(printf, "Add TS: %d\n", a);

	LOG(println, "Setup TimeSeries DB:");
	LOG_CALL(
		for (auto i : tsids) {
			auto t = this->getTS(i);
			if (t) {
				LOG(printf, "%s: size:%d, interval:%u, mem:%u\n"
					, t->getDescr()
					, t->capacity
					, t->getInterval()
					, t->capacity * sizeof(pz004::metrics)
				);
			}
		})

	LOG(printf, "SRAM: heap %u, free %u\n", ESP.getHeapSize(), ESP.getFreeHeap());
	LOG(printf, "SPI-RAM: size %u, free %u\n", ESP.getPsramSize(), ESP.getFreePsram());
}



template <class T>
////// return json-formatted response for in-RAM sampled data
void DataStorage<T>::wsamples(AsyncWebServerRequest *request) {
    uint8_t id = 1;	 // default ts id

    if (request->hasParam("tsid")) {
	const AsyncWebParameter *p = request->getParam("tsid");
	id = p->value().toInt();
    }

    // check if there is any sampled data
    if (!this->getTSsize(id)) {
	request->send(503, PGmimejson, "[]");
	return;
    }

	// json response maybe pretty large and needs too much of a precious ram to store it in a temp 'string'
	// So I'm going to generate it on-the-fly and stream to client in chunks

	size_t cnt = 0;	 // cnt - return last 'cnt' samples, 0 - all samples

	if (request->hasParam(C_scnt)) {
		const AsyncWebParameter *p = request->getParam(C_scnt);
		if (!p->value().isEmpty())
			cnt = p->value().toInt();
	}

	const auto ts = this->getTS(id);
	if (!ts){
		request->send(503, PGmimejson, "[]");
	}
	auto iter = ts->cbegin();  // get const iterator

	// set number of samples to send in responce
	if (cnt > 0 && cnt < ts->getSize()){
		iter += ts->getSize() - cnt;  // offset iterator to the last cnt elements
	}

	LOG(printf, "TimeSeries buffer has %d items, scntr: %d\n", ts->getSize(), cnt);

	AsyncWebServerResponse *response = request->beginChunkedResponse(FPSTR(PGmimejson),
		[this, iter, ts](uint8_t *buffer, size_t buffsize, size_t index) mutable -> size_t {
			// If provided bufer is not large enough to fit 1 sample chunk, than I'm just sending
			// an empty white space char (allowed json symbol) and wait for the next buffer
			if (buffsize < JSON_SMPL_LEN) {
				buffer[0] = 0x20;	// ASCII 'white space'
				return 1;
			}

			size_t len = 0;

			if (!index) {
				buffer[0] = 0x5b;	// Open json array with ASCII '['
				++len;
			}

			// prepare a chunk of sampled data wrapped in json
			while (len < (buffsize - JSON_SMPL_LEN) && iter != ts->cend()) {
				if (iter.operator->() != nullptr) {
					// obtain a copy of a struct (.asFloat() member method crashes for dereferenced obj - TODO: investigate)
					T m = *iter.operator->();

					len += sprintf((char *)buffer + len, PGsmpljsontpl
								, ts->getTstamp() - (ts->cend() - iter) * ts->getInterval()	// timestamp
								, m.asFloat(meter_t::vol)
								, m.asFloat(meter_t::cur)
								, m.asFloat(meter_t::pwr)
								, m.asFloat(meter_t::enrg) + nrg_offset
						                #ifdef G_B00_PZEM_MODEL_PZEM004V3
								    , m.asFloat(meter_t::frq)
								    , m.asFloat(meter_t::pf)
						                #endif
						);
				} else {
					LOG(println, "SMLP pointer is null");
				}

				if (++iter == ts->cend())
					buffer[len - 1] = 0x5d;  // ASCII ']' implaced over last comma
			}

			LOG(printf, "Sending timeseries JSON, buffer %d/%d, items left: %d\n"
				, len
				, buffsize
				, ts->cend() - iter
			);
			return len;
		});

	response->addHeader(PGacao, "*");  // CORS header
	request->send(response);
}

template <>
////// return json-formatted response for in-RAM sampled data
void DataStorage<pz004::metrics>::wsamples(AsyncWebServerRequest *request) {
    uint8_t id = 1;	 // default ts id

    if (request->hasParam("tsid")) {
	const AsyncWebParameter *p = request->getParam("tsid");
	id = p->value().toInt();
    }

    // check if there is any sampled data
    if (!this->getTSsize(id)) {
	request->send(503, PGmimejson, "[]");
	return;
    }

	// json response maybe pretty large and needs too much of a precious ram to store it in a temp 'string'
	// So I'm going to generate it on-the-fly and stream to client in chunks

	size_t cnt = 0;	 // cnt - return last 'cnt' samples, 0 - all samples

	if (request->hasParam(C_scnt)) {
		const AsyncWebParameter *p = request->getParam(C_scnt);
		if (!p->value().isEmpty())
			cnt = p->value().toInt();
	}

	const auto ts = this->getTS(id);
	if (!ts)
		request->send(503, PGmimejson, "[]");

	auto iter = ts->cbegin();  // get const iterator

	// set number of samples to send in responce
	if (cnt > 0 && cnt < ts->getSize())
		iter += ts->getSize() - cnt;  // offset iterator to the last cnt elements

	LOG(printf, "TimeSeries buffer has %d items, scntr: %d\n", ts->getSize(), cnt);

	AsyncWebServerResponse *response = request->beginChunkedResponse(FPSTR(PGmimejson),
		[this, iter, ts](uint8_t *buffer, size_t buffsize, size_t index) mutable -> size_t {
			// If provided bufer is not large enough to fit 1 sample chunk, than I'm just sending
			// an empty white space char (allowed json symbol) and wait for the next buffer
			if (buffsize < JSON_SMPL_LEN) {
				buffer[0] = 0x20;	// ASCII 'white space'
				return 1;
			}

			size_t len = 0;

			if (!index) {
				buffer[0] = 0x5b;	// Open json array with ASCII '['
				++len;
			}

			// prepare a chunk of sampled data wrapped in json
			while (len < (buffsize - JSON_SMPL_LEN) && iter != ts->cend()) {
				if (iter.operator->() != nullptr) {
					// obtain a copy of a struct (.asFloat() member method crashes for dereferenced obj - TODO: investigate)
					pz004::metrics m = *iter.operator->();

					len += sprintf((char *)buffer + len, PGsmpljsontpl
								, ts->getTstamp() - (ts->cend() - iter) * ts->getInterval()	// timestamp
								, m.asFloat(meter_t::vol)
								, m.asFloat(meter_t::cur)
								, m.asFloat(meter_t::pwr)
								, m.asFloat(meter_t::enrg) + nrg_offset
						                #ifdef G_B00_PZEM_MODEL_PZEM004V3
								    , m.asFloat(meter_t::frq)
								    , m.asFloat(meter_t::pf)
						                #endif
						);
				} else {
					LOG(println, "SMLP pointer is null");
				}

				if (++iter == ts->cend())
					buffer[len - 1] = 0x5d;  // ASCII ']' implaced over last comma
			}

			LOG(printf, "Sending timeseries JSON, buffer %d/%d, items left: %d\n"
				, len
				, buffsize
				, ts->cend() - iter
			);
			return len;
		});

	response->addHeader(PGacao, "*");  // CORS header
	request->send(response);
}

/////////////////////////////////////////////////////////////////////////

template <class T>
class Espem {
   public:
	#if defined(G_B00_PZEM_MODEL_PZEM003)
            PZ003	   *pz = nullptr;
	#elif defined(G_B00_PZEM_MODEL_PZEM004V3)
            PZ004	   *pz = nullptr;
	#endif

	// TimeSeries data storage
	DataStorage<T> ds;

	 // Class constructor
	 // uses predefined values of a ESPEM_CFG
	Espem() {
	}

	~Espem() {
		ts.deleteTask(t_uiupdater);
		delete pz;
		pz = nullptr;
		delete qport;
		qport = nullptr;
	}

	bool begin(const uart_port_t p, int rx = UART_PIN_NO_CHANGE, int tx = UART_PIN_NO_CHANGE);

	// @brief onNetIfUp - коллбек для внешнего события "сеть доступна" 
	void onNetIfUp();

	// @brief onNetIfDown - коллбек для внешнего события "сеть НЕ доступна"
	void onNetIfDown();

	// @brief - HTTP request callback with latest polled data (as json)
	void wdatareply(AsyncWebServerRequest *request);

	void wpmdata(AsyncWebServerRequest *request);

	 // @brief - set webUI refresh rate in seconds
	 // @param seconds - webUI interval
	uint8_t set_uirate(uint8_t seconds);

	// @brief Get the ui refresh rate
	// @return uint8_t
	uint8_t get_uirate();  // TaskScheduler class does not allow it to declare const'ness

	// @brief - Control meter polling
	// @param active - enable/disable
	// @return - current state
	bool	meterPolling(bool active) {
	    return pz->autopoll(active);
	}

	bool meterPolling() const {
	    return pz->autopoll();
	}

	mcstate_t set_collector_state(mcstate_t state);
	mcstate_t get_collector_state() const {
	    return ts_state;
	}

   private:
	UartQ	 *qport	   = nullptr;
	mcstate_t ts_state = mcstate_t::MC_DISABLE;
	// Tasks
	Task	  t_uiupdater;

	// mqtt feeder id
	int	_mqtt_feed_id{0};

	String	 &mktxtdata(String &txtdata);

	// @brief publish updates to websocket clients
	void	  wspublish();

	// make json string out of array provided
	// bool W - include energy counter in json
	// todo: provide vector with flags for each field
	// String& mkjsondata( const float result[], const unsigned long tstamp, String& jsn, const bool W );
};


template <>
class Espem<pz004::metrics> {
   public:
        #if defined(G_B00_PZEM_MODEL_PZEM003)
            PZ003	   *pz = nullptr;
	#elif defined(G_B00_PZEM_MODEL_PZEM004V3)
            PZ004	   *pz = nullptr;
	#endif
	
	// TimeSeries data storage
	DataStorage<pz004::metrics> ds;

	// Class constructor
	// uses predefined values of a ESPEM_CFG
	Espem(){};

	~Espem() {
		ts.deleteTask(t_uiupdater);
		delete pz;
		pz = nullptr;
		delete qport;
		qport = nullptr;
	}

	bool begin(const uart_port_t p, int rx = UART_PIN_NO_CHANGE, int tx = UART_PIN_NO_CHANGE);

	// @brief onNetIfUp - коллбек для внешнего события "сеть доступна" 
	void onNetIfUp();

	// @brief onNetIfDown - коллбек для внешнего события "сеть НЕ доступна"
	void onNetIfDown();

	// @brief - HTTP request callback with latest polled data (as json)
	void wdatareply(AsyncWebServerRequest *request);

	void wpmdata(AsyncWebServerRequest *request);

	 // @brief - set webUI refresh rate in seconds
	 // @param seconds - webUI interval
	uint8_t set_uirate(uint8_t seconds);

	// @brief Get the ui refresh rate
	// @return uint8_t
	uint8_t get_uirate();  // TaskScheduler class does not allow it to declare const'ness

	// @brief - Control meter polling
	// @param active - enable/disable
	// @return - current state
	bool	meterPolling(bool active) {
	    return pz->autopoll(active);
	};
	bool meterPolling() const {
	    return pz->autopoll();
	};

	mcstate_t set_collector_state(mcstate_t state);
	mcstate_t get_collector_state() const {
		return ts_state;
	};

   private:
	UartQ	 *qport	   = nullptr;
	mcstate_t ts_state = mcstate_t::MC_DISABLE;
	// Tasks
	Task	  t_uiupdater;

	// mqtt feeder id
	int		  _mqtt_feed_id{0};

	String	 &mktxtdata(String &txtdata);

	// @brief publish updates to websocket clients
	void	  wspublish();

	// make json string out of array provided
	// bool W - include energy counter in json
	// todo: provide vector with flags for each field
	// String& mkjsondata( const float result[], const unsigned long tstamp, String& jsn, const bool W );
};




// template <>
// void Espem<pz004::metrics>::wpmdata(AsyncWebServerRequest *request);

// template <>
// void Espem<pz004::metrics>::wdatareply(AsyncWebServerRequest *request);

// template <>
// void Espem<pz004::metrics>::wspublish();

// template <>
// uint8_t Espem<pz004::metrics>::get_uirate();


template <class T>
bool Espem<T>::begin(const uart_port_t p, int rx, int tx) {
	LOG(printf, "espem.begin: port: %d, rx_pin: %d, tx_pin:%d\n", p, rx, tx);

	// let's make our begin idempotent )
	if (qport) {
		if (pz){
			pz->detachMsgQ();
		}

		delete qport;
		qport = nullptr;
	}

	qport = new UartQ(p, rx, tx);
	if (!qport) return false;  // failed to create qport

	if (pz) {  // obj already exist
		pz->attachMsgQ(qport);
		qport->startQueues();
		return true;
	}


    #if defined(G_B00_PZEM_MODEL_PZEM003)
        #ifdef G_B00_PZEM_DUMMY
	        pz = new DummyPZ003(PZEM_ID, ADDR_ANY);
        #else
	        pz = new PZ003(PZEM_ID, ADDR_ANY);
        #endif
	#elif defined(G_B00_PZEM_MODEL_PZEM004V3)
	    #ifdef G_B00_PZEM_DUMMY
	        pz = new DummyPZ004(PZEM_ID, ADDR_ANY);
        #else
	        pz = new PZ004(PZEM_ID, ADDR_ANY);
        #endif
	#endif
	
	if (!pz) return false;	// failed to create obj

	pz->attachMsgQ(qport);
	qport->startQueues();

	// WebUI updater task
	t_uiupdater.set(DEFAULT_WS_UPD_RATE * TASK_SECOND, TASK_FOREVER, std::bind(&Espem::wspublish, this));
	ts.addTask(t_uiupdater);

	if (pz->autopoll(true)) {
		t_uiupdater.restartDelayed();
		LOG(println, "Autopolling enabled");
	} else {
		LOG(println, "Sorry, can't autopoll somehow :(");
	}

	embui.server.on(PSTR("/getdata"), HTTP_GET, [this](AsyncWebServerRequest *request) {
		wdatareply(request);
	});

	// compat method for v 1.x cacti scripts
	embui.server.on(PSTR("/getpmdata"), HTTP_GET, [this](AsyncWebServerRequest *request) {
		wpmdata(request);
	});

	// generate json with sampled meter data
	embui.server.on("/samples.json", HTTP_GET, [this](AsyncWebServerRequest *r) { ds.wsamples(r); });

	// create MQTT rawdata feeder and add into the chain
	_mqtt_feed_id = embui.feeders.add(std::make_unique<FrameSendMQTTRaw>(&embui));

	return true;
}

template <class T>
// make a string with last-polled data (cacti poller format)
// this is the 'compat' version for an old pzem w/o pf/HZ values
String &Espem<T>::mktxtdata(String &txtdata) {
	if (!pz)
		return txtdata;

	#if defined(G_B00_PZEM_MODEL_PZEM003)
            // pmeterData pdata = meter->getData();
	    const auto m = pz->getMetricsPZ003();
	
	//#if defined(G_B00_PZEM_MODEL_PZEM004V3)
        //    // pmeterData pdata = meter->getData();
	//    const auto m = pz->getMetricsPZ004();
	//#endif

	txtdata	 = "U:";
	txtdata += m->voltage / 10;
	txtdata += " I:";
	txtdata += m->current / 1000;
	txtdata += " P:";
	txtdata += m->asFloat(meter_t::pwr) + ds.getEnergyOffset();
	txtdata += " W:";
	txtdata += m->asFloat(meter_t::enrg);
	//    txtdata += " pf:";
	//    txtdata += pfcalc(meter->getData().meterings);
	#endif
	return txtdata;
}

// compat method for v 1.x cacti scripts
template <class T>
void Espem<T>::wpmdata(AsyncWebServerRequest *request) {
	if (!ds.getTSsize(1)) {
		request->send(503, PGmimetxt, PGdre);
		return;
	}

	String data;
	request->send(200, PGmimetxt, mktxtdata(data));
}

template <class T>
void Espem<T>::wdatareply(AsyncWebServerRequest *request) {
	if (!pz){
	    return;
	}

	#if defined(G_B00_PZEM_MODEL_PZEM003)
	    const auto m = pz->getMetricsPZ003();
	
	//#if defined(G_B00_PZEM_MODEL_PZEM004V3)
	//    const auto m = pz->getMetricsPZ004();
	//#endif
	//const auto m = pz->getMetricsPZ004();
	
	char	   buffer[JSON_SMPL_LEN];
	sprintf_P(buffer, PGdatajsontpl
				, pz->getState()->dataAge()
				, m->asFloat(meter_t::vol)
				, m->asFloat(meter_t::cur)
				, m->asFloat(meter_t::pwr)
				, m->asFloat(meter_t::enrg) + ds.getEnergyOffset()

				#if defined(G_B00_PZEM_MODEL_PZEM004V3)
					, m->asFloat(meter_t::frq)
					, m->asFloat(meter_t::pf)
				#endif
	        );
	request->send(200, FPSTR(PGmimejson), buffer);
	#endif
	
}

template <class T>
// publish meter data via availbale EmbUI feeders (a periodic Task)
void Espem<T>::wspublish() {
	if (!embui.feeders.available() || !pz){	// exit, if there are no clients connected
		return;
	}

	#if defined(G_B00_PZEM_MODEL_PZEM003)
	    const auto m = pz->getMetricsPZ003();
	
	//#if defined(G_B00_PZEM_MODEL_PZEM004V3)
	//    const auto m = pz->getMetricsPZ004();
	//#endif

	JsonDocument  doc;
	JsonObject obj  = doc.to<JsonObject>();
	doc["stale"]	= pz->getState()->dataStale();
	doc["age"]	= pz->getState()->dataAge();
	doc["U"]	= m->voltage;
	doc["I"]	= m->current;
	doc["P"]	= m->power;
	doc["W"]	= m->energy + ds.getEnergyOffset();
	// #if defined(G_B00_PZEM_MODEL_PZEM004V3)
	//   doc["Pf"]	= m->pf;
	//    doc["freq"]	= m->freq;
	// #endif

	Interface interf(&embui.feeders);
	// Interface interf(&embui.feeders, 128);
	interf.json_frame(C_espem);

	interf.json_frame_add(doc);
	// interf.jobject(doc, true);

	interf.json_frame_flush();
	#endif
}

template <class T>
uint8_t Espem<T>::set_uirate(uint8_t seconds) {
	if (seconds) {
		t_uiupdater.setInterval(seconds * TASK_SECOND);
		t_uiupdater.restartDelayed();
	} else
		t_uiupdater.disable();

	return seconds;
}

template <class T>
uint8_t Espem<T>::get_uirate() {
	if (t_uiupdater.isEnabled())
		return (t_uiupdater.getInterval() / TASK_SECOND);

	return 0;
}

template <class T>
mcstate_t Espem<T>::set_collector_state(mcstate_t state) {
	if (!pz) {
		ts_state = mcstate_t::MC_DISABLE;
		return ts_state;
	}

	switch (state) {
		case mcstate_t::MC_RUN: {
			if (ts_state == mcstate_t::MC_RUN) return mcstate_t::MC_RUN;
			if (!ds.getTScap()) ds.reset();	 // reinitialize TS Container if empty

			// attach collector's callback
			pz->attach_rx_callback([this](uint8_t id, const RX_msg *m) {
				// collect time-series data
				if (!pz->getState()->dataStale()) {
				    #if defined(G_B00_PZEM_MODEL_PZEM003)
					ds.push(*(pz->getMetricsPZ003()), time(nullptr));
				    #endif
				    // #elif defined(G_B00_PZEM_MODEL_PZEM004V3)
				    //    ds.push(*(pz->getMetricsPZ004()), time(nullptr));
				    // #endif
					
				}
				#ifdef ESPEM_DEBUG
					if (m) {
						msgdebug(id, m);
					}	 // it will print every data packet coming from PZEM
				#endif
			});
			ts_state = mcstate_t::MC_RUN;
			break;
		}
		case mcstate_t::MC_PAUSE: {
			pz->detach_rx_callback();
			ts_state = mcstate_t::MC_PAUSE;
			break;
		}
		default: {
			pz->detach_rx_callback();
			ds.purge();
			ts_state = mcstate_t::MC_DISABLE;
		}
	}
	return ts_state;
}

//////////


//template <>
bool Espem<pz004::metrics>::begin(const uart_port_t p, int rx, int tx) {
    LOG(printf, "espem.begin: port: %d, rx_pin: %d, tx_pin:%d\n", p, rx, tx);

	// let's make our begin idempotent )
	if (qport) {
		if (pz)
			pz->detachMsgQ();

		delete qport;
		qport = nullptr;
	}

	qport = new UartQ(p, rx, tx);
	if (!qport) return false;  // failed to create qport

	if (pz) {  // obj already exist
		pz->attachMsgQ(qport);
		qport->startQueues();
		return true;
	}


        #if defined(G_B00_PZEM_MODEL_PZEM003)
            #ifdef G_B00_PZEM_DUMMY
	        pz = new DummyPZ003(PZEM_ID, ADDR_ANY);
            #else
	        pz = new PZ003(PZEM_ID, ADDR_ANY);
            #endif
	#elif defined(G_B00_PZEM_MODEL_PZEM004V3)
	    #ifdef G_B00_PZEM_DUMMY
	        pz = new DummyPZ004(PZEM_ID, ADDR_ANY);
            #else
	        pz = new PZ004(PZEM_ID, ADDR_ANY);
            #endif
	#endif
	
	if (!pz) return false;	// failed to create obj

	pz->attachMsgQ(qport);
	qport->startQueues();

	// WebUI updater task
	t_uiupdater.set(DEFAULT_WS_UPD_RATE * TASK_SECOND, TASK_FOREVER, std::bind(&Espem::wspublish, this));
	ts.addTask(t_uiupdater);

	if (pz->autopoll(true)) {
		t_uiupdater.restartDelayed();
		LOG(println, "Autopolling enabled");
	} else {
		LOG(println, "Sorry, can't autopoll somehow :(");
	}

	embui.server.on(PSTR("/getdata"), HTTP_GET, [this](AsyncWebServerRequest *request) {
		wdatareply(request);
	});

	// compat method for v 1.x cacti scripts
	embui.server.on(PSTR("/getpmdata"), HTTP_GET, [this](AsyncWebServerRequest *request) {
		wpmdata(request);
	});

	// generate json with sampled meter data
	embui.server.on("/samples.json", HTTP_GET, [this](AsyncWebServerRequest *r) { ds.wsamples(r); });

	// create MQTT rawdata feeder and add into the chain
	_mqtt_feed_id = embui.feeders.add(std::make_unique<FrameSendMQTTRaw>(&embui));

	return true;
}



// make a string with last-polled data (cacti poller format)
// this is the 'compat' version for an old pzem w/o pf/HZ values
//template <>
String &Espem<pz004::metrics>::mktxtdata(String &txtdata) {
	if (!pz)
		return txtdata;

	//#if defined(G_B00_PZEM_MODEL_PZEM003)
        //    // pmeterData pdata = meter->getData();
	//    const auto m = pz->getMetricsPZ003();
	#if defined(G_B00_PZEM_MODEL_PZEM004V3)
            // pmeterData pdata = meter->getData();
	    const auto m = pz->getMetricsPZ004();
	

	  txtdata	 = "U:";
          txtdata += m->voltage / 10;
	  txtdata += " I:";
	  txtdata += m->current / 1000;
	  txtdata += " P:";
          txtdata += m->asFloat(meter_t::pwr) + ds.getEnergyOffset();
	  txtdata += " W:";
	  txtdata += m->asFloat(meter_t::enrg);
	  //    txtdata += " pf:";
	  //    txtdata += pfcalc(meter->getData().meterings);
	#endif
	return txtdata;
}



// compat method for v 1.x cacti scripts
//template <>
void Espem<pz004::metrics>::wpmdata(AsyncWebServerRequest *request) {
	if (!ds.getTSsize(1)) {
		request->send(503, PGmimetxt, PGdre);
		return;
	}

	String data;
	request->send(200, PGmimetxt, mktxtdata(data));
}

//template <>
void Espem<pz004::metrics>::wdatareply(AsyncWebServerRequest *request) {
	if (!pz){
	    return;
	}

	#if defined(G_B00_PZEM_MODEL_PZEM003)
	    const auto m = pz->getMetricsPZ003();
	#elif defined(G_B00_PZEM_MODEL_PZEM004V3)
	    const auto m = pz->getMetricsPZ004();
	#endif
	//const auto m = pz->getMetricsPZ004();
	
	char	   buffer[JSON_SMPL_LEN];
	sprintf_P(buffer, PGdatajsontpl
			  , pz->getState()->dataAge()
			  , m->asFloat(meter_t::vol)
			  , m->asFloat(meter_t::cur)
			  , m->asFloat(meter_t::pwr)
			  , m->asFloat(meter_t::enrg) + ds.getEnergyOffset()
		          
	                  #if defined(G_B00_PZEM_MODEL_PZEM004V3)
	                      , m->asFloat(meter_t::frq)
			      , m->asFloat(meter_t::pf)
	                  #endif
	                  );
	request->send(200, FPSTR(PGmimejson), buffer);
}

//template <>
// publish meter data via availbale EmbUI feeders (a periodic Task)
void Espem<pz004::metrics>::wspublish() {
	if (!embui.feeders.available() || !pz){	// exit, if there are no clients connected
		return;
	}

	#if defined(G_B00_PZEM_MODEL_PZEM003)
	    const auto m = pz->getMetricsPZ003();
	#elif defined(G_B00_PZEM_MODEL_PZEM004V3)
	    const auto m = pz->getMetricsPZ004();
	#endif

	JsonDocument  doc;
	JsonObject obj  = doc.to<JsonObject>();
	doc["stale"]	= pz->getState()->dataStale();
	doc["age"]	= pz->getState()->dataAge();
	doc["U"]	= m->voltage;
	doc["I"]	= m->current;
	doc["P"]	= m->power;
	doc["W"]	= m->energy + ds.getEnergyOffset();
	#if defined(G_B00_PZEM_MODEL_PZEM004V3)
	    doc["Pf"]	= m->pf;
	    doc["freq"]	= m->freq;
	#endif

	Interface interf(&embui.feeders);
	// Interface interf(&embui.feeders, 128);
	interf.json_frame(C_espem);

	interf.json_frame_add(doc);
	// interf.jobject(doc, true);

	interf.json_frame_flush();
}

//template <>
uint8_t Espem<pz004::metrics>::set_uirate(uint8_t seconds) {
	if (seconds) {
		t_uiupdater.setInterval(seconds * TASK_SECOND);
		t_uiupdater.restartDelayed();
	} else
		t_uiupdater.disable();

	return seconds;
}

//template <>
uint8_t Espem<pz004::metrics>::get_uirate() {
	if (t_uiupdater.isEnabled())
		return (t_uiupdater.getInterval() / TASK_SECOND);

	return 0;
}

//template <>
mcstate_t Espem<pz004::metrics>::set_collector_state(mcstate_t state) {
	if (!pz) {
		ts_state = mcstate_t::MC_DISABLE;
		return ts_state;
	}

	switch (state) {
		case mcstate_t::MC_RUN: {
			if (ts_state == mcstate_t::MC_RUN) return mcstate_t::MC_RUN;
			if (!ds.getTScap()) ds.reset();	 // reinitialize TS Container if empty

			// attach collector's callback
			pz->attach_rx_callback([this](uint8_t id, const RX_msg *m) {
				// collect time-series data
				if (!pz->getState()->dataStale()) {
				    // #if defined(G_B00_PZEM_MODEL_PZEM003)
	                            //     ds.push(*(pz->getMetricsPZ003()), time(nullptr));
	                            #if defined(G_B00_PZEM_MODEL_PZEM004V3)
	                                ds.push(*(pz->getMetricsPZ004()), time(nullptr));
	                            #endif	
				}
				#ifdef ESPEM_DEBUG
					if (m) {
						msgdebug(id, m);
					}	 // it will print every data packet coming from PZEM
				#endif
			});
			ts_state = mcstate_t::MC_RUN;
			break;
		}
		case mcstate_t::MC_PAUSE: {
			pz->detach_rx_callback();
			ts_state = mcstate_t::MC_PAUSE;
			break;
		}
		default: {
			pz->detach_rx_callback();
			ds.purge();
			ts_state = mcstate_t::MC_DISABLE;
		}
	}
	return ts_state;
}





void msgdebug(uint8_t id, const RX_msg *m) {
	Serial.printf("\nCallback triggered for PZEM ID: %d\n", id);

	// It is also possible to work directly on a raw data from PZEM
	// let's call for a little help here and use a pretty_printer() function
	// that parses and prints RX_msg to the stdout
	pz004::rx_msg_prettyp(m);
}
