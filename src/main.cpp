#include <Arduino.h>  //not needed in the arduino ide

// Captive Portal
#include <AsyncTCP.h>  //https://github.com/me-no-dev/AsyncTCP using the latest dev version from @me-no-dev
#include <DNSServer.h>
#include <ESPAsyncWebServer.h>	//https://github.com/me-no-dev/ESPAsyncWebServer using the latest dev version from @me-no-dev
#include <esp_wifi.h>			//Used for mpdu_rx_disable android workaround

// Pre reading on the fundamentals of captive portals https://textslashplain.com/2022/06/24/captive-portals/

const char *ssid = "Free Wifi";  // FYI The SSID can't have a space in it.
// const char * password = "12345678"; //Atleast 8 chars
const char *password = NULL;  // no password

#define MAX_CLIENTS 4	// ESP32 supports up to 10 but I have not tested it yet
#define WIFI_CHANNEL 6	// 2.4ghz channel 6 https://en.wikipedia.org/wiki/List_of_WLAN_channels#2.4_GHz_(802.11b/g/n/ax)

const IPAddress localIP(4, 3, 2, 1);		   // the IP address the web server, Samsung requires the IP to be in public space
const IPAddress gatewayIP(4, 3, 2, 1);		   // IP address of the network should be the same as the local IP for captive portals
const IPAddress subnetMask(255, 255, 255, 0);  // no need to change: https://avinetworks.com/glossary/subnet-mask/

const String localIPURL = "http://4.3.2.1";	 // a string version of the local IP with http, used for redirecting clients to your webpage

int rickroll_count = 0;

const char index_html_part_1[] PROGMEM = R"=====(
  <!DOCTYPE html> <html>
    <head>
      <title>get rickrolled lmao</title>
      <meta name="viewport" content="width=device-width, initial-scale=1.0">
    </head>
    <body>
      <h1>never gonna give you up</h1>
	  <h2>never gonna let you down</h2>
	  <h3>never gonna run around and desert you</h3>
	  <h4>never gonna make you cry</h4>
	  <h5>never gonna say goodbye</h5>
	  <h6>never gonna tell a lie and hurt you</h6>
	  <p>so far )=====";

const char index_html_part_2[] PROGMEM = R"=====( people have been rickrolled (please note this may not be accurate, due to refreshes)</p>
	  <p>fun fact! this entire wifi network is all running on a single esp32 in someones backpack</p>
	  <p>i hope now you know not to trust public wifi networks, they can be used to steal your data</p>
	  <p>if you want to learn more about this, check out the links below</p>
	  <ul>
	  	<li><a href="https://www.forbes.com/advisor/business/public-wifi-risks/">https://www.forbes.com/advisor/business/public-wifi-risks/</a></li>
		<li><a href="https://www.uab.edu/it/news/item/public-wifi-not-always-convenient-and-could-be-dangerous">https://www.uab.edu/it/news/item/public-wifi-not-always-convenient-and-could-be-dangerous</a></li>
	  </ul>
	  <br><br>
	  <hr>
	  <p>built with love by that weird person in the corner you were sure wasnt there before</p>
	</body>
  </html>
)=====";

const char admin_html_part_1[] PROGMEM = R"=====(
	<!DOCTYPE html> <html>
		<head>
			<title>the rickroll ap admin panel</title>
			<meta name="viewport" content="width=device-width, initial-scale=1.0">
		</head>
		<body>
			<h1>the rickroll ap admin panel</h1>
			<p>you can use this to monitor the rickroll ap</p>
			<script>
				setTimeout(function(){
					window.location.reload(1);
				}, 2000);
			</script>
			<h2>rickroll stats</h2>
			<p>so far )=====";

const char admin_html_part_2[] PROGMEM = R"=====( people have been rickrolled</p>
			<p>the current temperature of the esp32 is )=====";

const char admin_html_part_3[] PROGMEM = R"=====( degrees celsius</p>
			<br>
			<p><i>if you are not the owner of this device, you should probably leave</i></p>
			<p>go to the <a href="/">rickroll page</a> please</p>
			<p>are you still here? you should probably leave</p>
			<br>
			<p>are you really still here? you should probably leave</p>
			<p>you know what? i'm not going to stop you</p>
			<p>you can stay here if you want</p>
			<p>but you should probably leave</p>
			<br>
			<p>are you really still here? god damn it</p>
			<p>go get rickrolled already</p>
			<hr>
			<p>built with love by that weird person in the corner you were sure wasnt there before</p>
		</body>
	</html>
)=====";

String get_full_html() {
	String full_html = String(index_html_part_1) + String(rickroll_count) + String(index_html_part_2);
	return full_html;
}

String get_admin_html() {
	String admin_html = String(admin_html_part_1) + String(rickroll_count) + String(admin_html_part_2) + String(temperatureRead()) + String(admin_html_part_3);
	return admin_html;
}

DNSServer dnsServer;
AsyncWebServer server(80);

void setUpDNSServer(DNSServer &dnsServer, const IPAddress &localIP) {
// Define the DNS interval in milliseconds between processing DNS requests
#define DNS_INTERVAL 30

	// Set the TTL for DNS response and start the DNS server
	dnsServer.setTTL(3600);
	dnsServer.start(53, "*", localIP);
}

void startSoftAccessPoint(const char *ssid, const char *password, const IPAddress &localIP, const IPAddress &gatewayIP) {
// Define the maximum number of clients that can connect to the server
#define MAX_CLIENTS 4
// Define the WiFi channel to be used (channel 6 in this case)
#define WIFI_CHANNEL 6

	// Set the WiFi mode to access point and station
	WiFi.mode(WIFI_MODE_AP);

	// Define the subnet mask for the WiFi network
	const IPAddress subnetMask(255, 255, 255, 0);

	// Configure the soft access point with a specific IP and subnet mask
	WiFi.softAPConfig(localIP, gatewayIP, subnetMask);

	// Start the soft access point with the given ssid, password, channel, max number of clients
	WiFi.softAP(ssid, password, WIFI_CHANNEL, 0, MAX_CLIENTS);

	// Disable AMPDU RX on the ESP32 WiFi to fix a bug on Android
	esp_wifi_stop();
	esp_wifi_deinit();
	wifi_init_config_t my_config = WIFI_INIT_CONFIG_DEFAULT();
	my_config.ampdu_rx_enable = false;
	esp_wifi_init(&my_config);
	esp_wifi_start();
	vTaskDelay(100 / portTICK_PERIOD_MS);  // Add a small delay
}

void setUpWebserver(AsyncWebServer &server, const IPAddress &localIP) {
	//======================== Webserver ========================
	// WARNING IOS (and maybe macos) WILL NOT POP UP IF IT CONTAINS THE WORD "Success" https://www.esp8266.com/viewtopic.php?f=34&t=4398
	// SAFARI (IOS) IS STUPID, G-ZIPPED FILES CAN'T END IN .GZ https://github.com/homieiot/homie-esp8266/issues/476 this is fixed by the webserver serve static function.
	// SAFARI (IOS) there is a 128KB limit to the size of the HTML. The HTML can reference external resources/images that bring the total over 128KB
	// SAFARI (IOS) popup browser has some severe limitations (javascript disabled, cookies disabled)

	// Required
	server.on("/connecttest.txt", [](AsyncWebServerRequest *request) { request->redirect("http://logout.net"); });	// windows 11 captive portal workaround
	server.on("/wpad.dat", [](AsyncWebServerRequest *request) { request->send(404); });								// Honestly don't understand what this is but a 404 stops win 10 keep calling this repeatedly and panicking the esp32 :)

	// Background responses: Probably not all are Required, but some are. Others might speed things up?
	// A Tier (commonly used by modern systems)
	server.on("/generate_204", [](AsyncWebServerRequest *request) { request->redirect(localIPURL); });		   // android captive portal redirect
	server.on("/redirect", [](AsyncWebServerRequest *request) { request->redirect(localIPURL); });			   // microsoft redirect
	server.on("/hotspot-detect.html", [](AsyncWebServerRequest *request) { request->redirect(localIPURL); });  // apple call home
	server.on("/canonical.html", [](AsyncWebServerRequest *request) { request->redirect(localIPURL); });	   // firefox captive portal call home
	server.on("/success.txt", [](AsyncWebServerRequest *request) { request->send(200); });					   // firefox captive portal call home
	server.on("/ncsi.txt", [](AsyncWebServerRequest *request) { request->redirect(localIPURL); });			   // windows call home

	// B Tier (uncommon)
	//  server.on("/chrome-variations/seed",[](AsyncWebServerRequest *request){request->send(200);}); //chrome captive portal call home
	//  server.on("/service/update2/json",[](AsyncWebServerRequest *request){request->send(200);}); //firefox?
	//  server.on("/chat",[](AsyncWebServerRequest *request){request->send(404);}); //No stop asking Whatsapp, there is no internet connection
	//  server.on("/startpage",[](AsyncWebServerRequest *request){request->redirect(localIPURL);});

	// return 404 to webpage icon
	server.on("/favicon.ico", [](AsyncWebServerRequest *request) { request->send(404); });	// webpage icon

	// Serve Basic HTML Page
	server.on("/", HTTP_ANY, [](AsyncWebServerRequest *request) {
		rickroll_count++;
		AsyncWebServerResponse *response = request->beginResponse(200, "text/html", get_full_html());
		//response->addHeader("Cache-Control", "public,max-age=31536000");  // save this file to cache for 1 year (unless you refresh)
		request->send(response);
		Serial.println("A new client has been rickrolled!");
		Serial.print("Rickroll Count: ");
		Serial.println(rickroll_count);
	});

	// Serve Admin Page
	server.on("/admin", HTTP_ANY, [](AsyncWebServerRequest *request) {
		AsyncWebServerResponse *response = request->beginResponse(200, "text/html", get_admin_html());
		//response->addHeader("Cache-Control", "public,max-age=31536000");  // save this file to cache for 1 year (unless you refresh)
		request->send(response);
	});

	// the catch all
	server.onNotFound([](AsyncWebServerRequest *request) {
		request->redirect(localIPURL);
		Serial.print("onnotfound ");
		Serial.print(request->host());	// This gives some insight into whatever was being requested on the serial monitor
		Serial.print(" ");
		Serial.print(request->url());
		Serial.print(" sent redirect to " + localIPURL + "\n");
	});
}

void setup() {
	// Set the transmit buffer size for the Serial object and start it with a baud rate of 115200.
	Serial.setTxBufferSize(1024);
	Serial.begin(115200);

	// Wait for the Serial object to become available.
	while (!Serial)
		;

	// Print a welcome message to the Serial port.
	Serial.println("\n\nrickroll, V0.1.0 compiled " __DATE__ " " __TIME__);  //__DATE__ is provided by the platformio ide
	Serial.printf("%s-%d\n\r", ESP.getChipModel(), ESP.getChipRevision());

	startSoftAccessPoint(ssid, password, localIP, gatewayIP);

	setUpDNSServer(dnsServer, localIP);

	setUpWebserver(server, localIP);
	server.begin();

	Serial.print("\n");
	Serial.print("Startup Time:");	// should be somewhere between 270-350 for Generic ESP32 (D0WDQ6 chip, can have a higher startup time on first boot)
	Serial.println(millis());
	Serial.print("\n");
}

void loop() {
	dnsServer.processNextRequest();	 // I call this atleast every 10ms in my other projects (can be higher but I haven't tested it for stability)
	delay(DNS_INTERVAL);			 // seems to help with stability, if you are doing other things in the loop this may not be needed
}