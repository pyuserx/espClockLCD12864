

#include <ArduinoJson.h>
#include <ESP8266WiFi.h>
#include <ESP8266HTTPClient.h>
#include <ESP8266WebServer.h>
#include <WiFiClientSecure.h>
#include <WiFiManager.h>
#include <U8g2lib.h>     
// #include <ArduinoHttpClient.h>
#include <WiFiUdp.h>
#include <NTPClient.h>  

#include <EEPROM.h>

#define AP_NAME "ESP8266"
// 配置结构体 - 便于持久化存储
struct Config {
  char province[20];
  char city[20];

  char ssid[20];
  char passwd[20];

  uint16_t updateInterval;
  uint16_t check;
};
Config config;

#define EEPROM_SIZE sizeof(config)
#define CUSTOM_DATA_START 0

bool shouldSaveConfig = false;
bool weaUpdate = false;
//屏幕库
// U8G2_SSD1306_128X64_NONAME_F_HW_I2C u8g2(U8G2_R0 , U8X8_PIN_NONE); 
// U8G2_ST7567_OS12864_1_4W_HW_SPI u8g2xx(
//   U8G2_R0,       // 旋转方向
//   /*cs=*/ 4,    // GPIO15 (片选)
//   /*dc=*/ 5,     // GPIO2 (数据/命令)
//   /*rst=*/ 16     // GPIO4 (复位)
// );

// U8G2_ST7567_JLX12864_F_4W_SW_SPI u8g2(U8G2_R2, 14, 13, 4, 5, 16);

U8G2_ST7567_JLX12864_2_4W_HW_SPI u8g2(
  U8G2_R0,       // 旋转方向
  /*cs=*/ 4,    // GPIO15 (片选)
  /*dc=*/ 5,     // GPIO2 (数据/命令)
  /*rst=*/ 16     // GPIO4 (复位)
);

// ESP8266WiFi WiFi;
WiFiClient client;
WiFiManager wm;
//声明
#define CONFIG_BUTTON_PIN 16

String weather = "--";
String temperature = "--";
String humidity = "--";
String scrollText;
String alarmText;
String comfortText;

WiFiUDP ntpUDP;
NTPClient timeClient(ntpUDP, "time.apple.com", 8*3600, 60000); // 东八区时间(+8小时)

void httpClientRequest();
void mainDisplay(int) ;

void DisplayConfig(void)
{
  u8g2.clearBuffer();// 清空显示设备内部缓冲区
  u8g2.setFont(u8g2_font_wqy12_t_gb2312);

  do
  {

    u8g2.enableUTF8Print();//enable UTF8
    u8g2.setCursor(0, 10);
    char buff[20] = {0};
    strcat(buff, "Wi-Fi: ");
    strcat(buff, AP_NAME);
    u8g2.print(buff);
    u8g2.setCursor(0, 25);
    u8g2.print(">192.168.4.1");
    u8g2.setCursor(0, 40);
    u8g2.print(">请连接WIFI并配置信息");
    // u8g2.setCursor(0, 55);
    // u8g2.print("【恶魔工作室】");

  }

  while (u8g2.nextPage());
}

void configModeCallback(WiFiManager *wm) {
  Serial.println("进入配置模式");
  Serial.print("AP IP: ");
  Serial.println(WiFi.softAPIP());
  Serial.print("SSID: ");
  Serial.println(wm->getConfigPortalSSID());
}

bool loadConfig() {

  
  EEPROM.begin(EEPROM_SIZE);
  EEPROM.get(CUSTOM_DATA_START, config);
  EEPROM.end();
  
  // 验证配置是否为空
  if (config.check == 0xabcd){
    
    Serial.println("\n使用默认配置:");

    Serial.println(config.province); 
    Serial.println(config.city); 
    Serial.println(config.ssid); 
    // Serial.println(config.passwd); 
    Serial.println(config.updateInterval); 
    return true;
  }
  else
  {
    Serial.println("\n请先配置WiFi。");
  }
  return false;
}
void saveConfigCallback() {
  Serial.println("保存配置...");
  config.check = 0xabcd;
  // 保存到EEPROM
  EEPROM.begin(EEPROM_SIZE);
  EEPROM.put(CUSTOM_DATA_START, config);
  EEPROM.commit();
  EEPROM.end();
  delay(100);
  shouldSaveConfig = true;
  Serial.println("配置已保存到EEPROM");

  memset(config.city, 0, sizeof(config.city));
  loadConfig();
}
bool ParameterInit = false;
void autoConfig() {

  // 加载保存的配置
  // if(loadConfig() == false)
  {
    

    // wm.erase();
    shouldSaveConfig = false;
    // 设置保存配置回调
    // wm.setSaveConfigCallback([]() {
    //   Serial.println("配置门户超时，正在重启...");
    //   shouldSaveConfig = true;
    // });
    // 设置回调
    wm.setSaveConfigCallback(saveConfigCallback);
    wm.setAPCallback(configModeCallback);
    
    // 设置自定义HTML
    // wm.setCustomHeadElement(FPSTR(customHead));
    // wm.setCustomHeadElement(customHTML);
    
    // 创建自定义参数（使用预分配缓冲区）

    static WiFiManagerParameter custom_province("province", "省份", config.province, sizeof(config.province));
    static WiFiManagerParameter custom_city("city", "城市", config.city, sizeof(config.city));

    char interval_str[6];
    sprintf(interval_str, "%d", config.updateInterval);
    static WiFiManagerParameter custom_interval("interval", "数据更新间隔(分)", interval_str, 6);
    
    // WiFiManagerParameter custom_html_break("<hr>");
    

    
    // 设置配置门户超时时间
    wm.setConfigPortalTimeout(180);

    if(ParameterInit == false)
    {
      ParameterInit = true;
      // 添加参数到WiFiManager
      // wm.addParameter(&custom_html_break);
    
      wm.addParameter(&custom_province);
      wm.addParameter(&custom_city);

      // wm.addParameter(&custom_html_break);
      wm.addParameter(&custom_interval);
      // wm.addParameter(&custom_html_break);
    }
    
    // if(reset == true)
    // {
    //   wm.startWebPortal();
    // }
    // 启动非阻塞配置门户
    if (!wm.autoConnect(AP_NAME)) {
      Serial.println("配置门户超时，正在重启...");
      delay(3000);
      ESP.restart();
    }
    // 只有在配置变更时才保存
    if (shouldSaveConfig) {
      strncpy(config.province, custom_province.getValue(), 20);
      // 配置完成后更新参数
      strncpy(config.city, custom_city.getValue(), 20);
      config.updateInterval = atoi(custom_interval.getValue());


      strncpy(config.ssid, wm.getWiFiSSID().c_str(), 20);
      strncpy(config.passwd, wm.getWiFiPass().c_str(), 20);

      Serial.println(custom_province.getValue());
      Serial.println(custom_city.getValue());

      Serial.println(wm.getWiFiSSID());
      Serial.println(wm.getWiFiPass());    
      // 保存配置
      saveConfigCallback();


      Serial.println("新配置已保存");
    } else {
      Serial.println("使用已保存的配置");
    }

    weaUpdate = true;
    Serial.println("已连接到WiFi!");
    Serial.print("IP地址: ");
    Serial.println(WiFi.localIP());
  }
}
String urlEncode(const char* msg) {
  const char *hex = "0123456789ABCDEF";
  String encodedMsg = "";
  while (*msg != '\0') {
    if( ('a' <= *msg && *msg <= 'z')
        || ('A' <= *msg && *msg <= 'Z')
        || ('0' <= *msg && *msg <= '9')
        || *msg == '-' || *msg == '_' || *msg == '.' || *msg == '~') {
      encodedMsg += *msg;
    } else {
      encodedMsg += '%';
      encodedMsg += hex[*msg >> 4];
      encodedMsg += hex[*msg & 15];
    }
    msg++;
  }
  return encodedMsg;
}

// 发送HTTP请求并且将服务器响应通过串口输出
void httpClientRequest(String province, String city) {


  // String weaurl = "https://i.news.qq.com/weather/common?source=pc&weather_type=observe%7Cindex%7Calarm%7Climit&province=";
  String weaurl = "https://i.news.qq.com/weather/common?source=pc&weather_type=observe%7Calarm%7Climit&province=";
  weaurl += urlEncode(province.c_str());
  weaurl += "&city=";
  weaurl += urlEncode(city.c_str());
  weaurl += "&county=";


  // test
  // String weaurl = "https://i.news.qq.com/weather/common?source=pc&weather_type=observe%7Cindex%7Calarm%7Climit&province=%E6%96%B0%E7%96%86&city=%E4%B9%8C%E9%B2%81%E6%9C%A8%E9%BD%90&county=";
  // weaurl = "https://i.news.qq.com/weather/common?source=pc&weather_type=observe%7Calarm%7Climit&province=%E6%96%B0%E7%96%86&city=%E4%B9%8C%E9%B2%81%E6%9C%A8%E9%BD%90&county=";
  WiFiClientSecure client;
  //重点1 创建 HTTPClient 对象
  HTTPClient httpClient;
  client.setInsecure();
  //重点2 通过begin函数配置请求地址。此处也可以不使用端口号和PATH而单纯的
  httpClient.begin(client, weaurl);
  Serial.print("\nURL: "); Serial.println(weaurl);

  //重点3 通过GET函数启动连接并发送HTTP请求
  int httpCode = httpClient.GET();


  //重点4. 如果服务器响应HTTP_CODE_OK(200)则从服务器获取响应体信息并通过串口输出
  //如果服务器不响应HTTP_CODE_OK(200)则将服务器响应状态码通过串口输出
  if (httpCode == HTTP_CODE_OK) {
    // 使用getString函数获取服务器响应体内容
    Serial.println("HTTP_CODE_OK");

    String responsePayload = httpClient.getString();
    Serial.print("Server Response Payload: ");
    Serial.println(responsePayload);

    Serial.print("Free heap: ");
    Serial.println(ESP.getFreeHeap());

    // size_t requiredSize = measureJson(responsePayload.c_str());
    // Serial.print("需要的内存: ");
    // Serial.println(requiredSize);
    // 使用estimateJsonSize()计算实际需要的大小
    
    // const size_t neededSize = estimateJsonSize(responsePayload.length());
    // StaticJsonDocument<neededSize + 50> doc; // 额外增加50字节缓冲
    DynamicJsonDocument doc(1000 + 128);
    // StaticJsonDocument<10000> doc; // 根据JSON大小调整缓冲区
    DeserializationError error = deserializeJson(doc, responsePayload);
    // StaticJsonDocument<10000> doc; // 根据JSON大小调整缓冲区
    // DeserializationError error = deserializeJson(doc, httpClient.getString());

    if (error) {
      Serial.print("解析失败: ");
      Serial.println(error.c_str());
      return;
    }
    // const char *p = doc["data"]["observe"]["humidity"];
    // humidity = String(p);
    humidity = String(doc["data"]["observe"]["humidity"]);
    temperature = String(doc["data"]["observe"]["degree"]);
    // p = doc["data"]["index"]["comfort"]["detail"];
    comfortText = String(doc["data"]["index"]["comfort"]["detail"]);
    // comfortText = String(p);
    weather = String(doc["data"]["observe"]["weather"]);

    Serial.print("comfortText:");
    Serial.println(comfortText);

    alarmText = String(doc["data"]["alarm"][0]["detail"]);
    Serial.print("alarmText:");
    Serial.println(alarmText);

    Serial.print("温度：");
    Serial.println(temperature);

    Serial.print("天气：");
    Serial.println(weather);

    // Serial.print("风向：");
    // Serial.println(winddirection);

    Serial.print("湿度：");
    Serial.println(humidity);

  } else {
    Serial.println("Server Respose Code：");
    Serial.println(httpCode);
  }

  //重点5. 关闭ESP8266与服务器连接
  httpClient.end();

  WiFi.disconnect(false);
  WiFi.mode(WIFI_OFF);
  Serial.println("WiFi已关闭");
}
int scrollIndex;
int lastCheckTime;

/**
 * 从UTF-8字符串中截取指定位置和长度的子字符串
 * @param src 源字符串
 * @param pos 起始位置（从0开始，按字符计数）
 * @param len 要截取的字符长度
 * @param dest 目标缓冲区
 * @param destSize 目标缓冲区大小
 * @return 实际截取的字符数
 */
size_t utf8_substr(const char* src, size_t pos, size_t len, char* dest, size_t destSize) {
  if (src == nullptr || dest == nullptr || destSize < 1) return 0;
  
  dest[0] = '\0';  // 初始化目标缓冲区
  
  // 跳过pos个字符
  const char* p = src;
  size_t charCount = 0;
  
  while (*p && charCount < pos) {
    // 获取当前UTF-8字符的字节长度
    uint8_t firstByte = *p;
    size_t byteLen = 1;
    
    if ((firstByte & 0xF8) == 0xF0) byteLen = 4;
    else if ((firstByte & 0xF0) == 0xE0) byteLen = 3;
    else if ((firstByte & 0xE0) == 0xC0) byteLen = 2;
    
    // 跳过当前字符
    p += byteLen;
    charCount++;
  }
  
  // 开始复制字符，直到达到len或字符串结束
  
  charCount = 0;
  size_t bytesCopied = 0;
  
  while (*p && charCount < len) {
    // 获取当前UTF-8字符的字节长度
    uint8_t firstByte = *p;
    size_t byteLen = 1;
    
    if ((firstByte & 0xF8) == 0xF0) byteLen = 4;
    else if ((firstByte & 0xF0) == 0xE0) byteLen = 3;
    else if ((firstByte & 0xE0) == 0xC0) byteLen = 2;
    
    // 检查是否有足够的空间
    if (bytesCopied + byteLen + 1 > destSize) break;
    
    // 复制当前字符
    memcpy(dest + bytesCopied, p, byteLen);
    bytesCopied += byteLen;
    charCount++;
    p += byteLen;
  }
  
  // 添加字符串结束符
  if (bytesCopied < destSize) {
    dest[bytesCopied] = '\0';
  } else if (destSize > 0) {
    dest[destSize - 1] = '\0';
  }
  
  return charCount;
}
/**
 * 优化的UTF-8字符串长度计算函数
 * 使用位操作提高性能
 */
size_t utf8_strlen(const char* str) {
    size_t len = 0;
    while (*str) {
        len++;
        // 快速跳过多字节字符的后续字节
        if ((*str++ & 0xC0) == 0xC0) {
            while ((*str & 0xC0) == 0x80) str++;
        }
    }
    return len;
}
int retry = 0;
void setup() {
  pinMode(CONFIG_BUTTON_PIN, INPUT_PULLUP);
  Serial.begin(115200);


  delay(500);
  u8g2.begin();
  u8g2.setContrast(5);
  // u8g2.clearBuffer();// 清空显示设备内部缓冲区
  // u8g2.setFont(u8g2_font_wqy12_t_gb2312);



  // // return;

  // // Serial.println("加载完成");
  // u8g2.firstPage();
  // do
  // {

  //   u8g2.enableUTF8Print();//enable UTF8
  //   u8g2.setCursor(0, 10);
  //   char buff[20] = {0};
  //   strcat(buff, "Wi-Fi: ");
  //   strcat(buff, AP_NAME);
  //   u8g2.print(buff);
  //   u8g2.setCursor(0, 25);
  //   u8g2.print(">192.168.4.1");
  //   u8g2.setCursor(0, 40);
  //   u8g2.print(">请连接WIFI并配置信息");
  //   // u8g2.setCursor(0, 55);
  //   // u8g2.print("【恶魔工作室】");

  // }

  // while (u8g2.nextPage());
  DisplayConfig();

  if (loadConfig() == false)
  {
    // 开启热点配置          
    // 连接WiFi
    // 建立WiFiManager对象
    autoConfig();  
  }
  else
  {
    WiFi.mode(WIFI_STA);
    WiFi.begin(config.ssid, config.passwd);
    while (WiFi.status() != WL_CONNECTED && retry < 10) {
      delay(1000);
      Serial.print(".");
      retry++;
    }
  }
  timeClient.update();
 
  // if(WiFi.status() == WL_CONNECTED) {
  //   // 已连接的处理逻辑
  //   Serial.println("WiFi connected");
  // } else {
  //   // 未连接的处理逻辑
  //   Serial.println("WiFi disconnected");
  //   // startConfigMode();
  // }


  // sprintf(config.city, "%s", "深圳");
  Serial.println(config.city);
  // httpClientRequest(String(config.province),String(config.city));
  // httpClientRequest("深圳");
  // httpClientRequest("shenzhen");
  timeClient.update();
  // 同步到内置RTC
  // system_rtc_mem_write(64, &timeClient.getEpochTime(), 4);
  //connectWiFi();
  lastCheckTime = config.updateInterval*60;
}


int year ;
int month;
int day;
int hour;
int minute;
int second;

int disIndex;
int switchTime = 0;
void loop() {

  if (lastCheckTime > config.updateInterval*60 || weaUpdate) { 
  // if (lastCheckTime > 1*60) { 
  // if(0){
    weaUpdate = false;
    // lastCheckTime = 0;
    Serial.printf("刷新：");
    WiFi.mode(WIFI_STA);
    WiFi.begin(config.ssid, config.passwd);
    while (WiFi.status() != WL_CONNECTED && retry < 10) {
      delay(1000);
      Serial.print(".");
      retry++;
    }
    retry = 0;
    timeClient.update();
    httpClientRequest(String(config.province),String(config.city)); 
  
    WiFi.disconnect(false);
    WiFi.mode(WIFI_OFF);
    Serial.println("WiFi已关闭");
    lastCheckTime = 0;
  }


  // 获取完整时间戳
  // unsigned long epochTime = timeClient.getEpochTime();

  // unsigned long rawTime = timeClient.getEpochTime();
  // unsigned long hours = (rawTime % 86400L) / 3600;

  // if (timeClient.update())
  // timeClient.update();
  {

      // timeClient.update();
    time_t epochTime = timeClient.getEpochTime();
    
    // Serial.print("Epoch: ");
    // Serial.println(epochTime); // 应显示10位正整数
    
    struct tm *ptm = gmtime(&epochTime);
    // Serial.printf("%04d-%02d-%02d\n", 
    // ptm->tm_year + 1900, 
    // ptm->tm_mon + 1,
    // ptm->tm_mday);


    year = ptm->tm_year + 1900;  // 年份需要加上1900
    month = ptm->tm_mon + 1;     // 月份从0开始，所以加1
    day = ptm->tm_mday;          // 日期
    hour = timeClient.getHours(); 
    minute = timeClient.getMinutes(); 
    second = timeClient.getSeconds(); 
  }

  // 30s
  if (++switchTime < 30*2)
  {
    disIndex = 0;
    scrollIndex = 0;

    if ((alarmText.length() < 5) && (comfortText.length() < 5))
    {
      switchTime = 0;
    }
    
  }
  else  if (switchTime < 90*2)
  {
    scrollText = alarmText;
    if (disIndex == 0)
    {
      Serial.println(scrollText);
    }
    
    disIndex = 1;
    
    // if (alarmText == "null")
    if (alarmText.length() < 5)
    {
      disIndex = 2;
      scrollIndex = 0;
      scrollText = comfortText;
      switchTime = 90*2;
    }
  }
  else  if (switchTime < (90+30)*2)
  {
    scrollText = comfortText;
    if (disIndex != 2)
    {
      Serial.println(scrollText);
      scrollIndex = 0;
    }
    disIndex = 2;
    
    
    if (comfortText.length() < 5)
    {
      disIndex = 0;
      switchTime = 0;
    }
  }
  else
  {
    switchTime = 0;
    disIndex = 0;
  }
  
  mainDisplay(disIndex);

  //循环

  // 检查按钮是否按下（长按3秒进入配置模式）
  static bool buttonActive = false;
  static unsigned long buttonPressTime = 0;
  
  if (digitalRead(CONFIG_BUTTON_PIN) == LOW) {
    if (!buttonActive) {
      buttonActive = true;
      buttonPressTime = millis();
      Serial.println("Button pressed");
    }
    
    // 如果按下时间超过3秒
    if (buttonActive && (millis() - buttonPressTime > 3000)) {
      Serial.println("Button held for reset");
      // startConfigurationPortal();
      DisplayConfig();
      autoConfig();
    }
  } else if (buttonActive) {
    buttonActive = false;
  }
  delay(500);
  lastCheckTime++;

  

}

const char* week[7] = {
  "周日",
  "周一",
  "周二",
  "周三",
  "周四",
  "周五",
  "周六",

};

// const char* scrollText = "gdfjdfkfyuor6idthjfgkhfdsgwaetershbcnxfgnfgnzfhdghdfg";
int lencity,lenwea;
int textWidth = 16;
void mainDisplay(int index) {
  char buff[50];
  u8g2.firstPage();
  do
  {
    String hoursStr = hour < 10 ? "0" + String(hour) : String(hour);
    String minuteStr = minute < 10 ? "0" + String(minute) : String(minute);
    String secondStr = second < 10 ? "0" + String(second) : String(second);
    String hm = hoursStr + ":" + minuteStr;
    String sec = ":" + secondStr;

    // 格式化输出
    char dateStr[20];

    sprintf(dateStr, "%04d-%02d-%02d", year, month, day);

    u8g2.enableUTF8Print();//enable UTF8

    u8g2.setCursor(0, 11);
    // u8g2.setFont(u8g2_font_courR12_tf);
    u8g2.setFont(u8g2_font_wqy12_t_gb2312);
    u8g2.print(dateStr);


    u8g2.setCursor(105, 11);
    u8g2.print(week[timeClient.getDay()]);


    switch (index)
    {
      // 显示天气
      case 0:

        u8g2.setFont(u8g2_font_wqy12_t_gb2312);

        // 判断文字长度，调整显示宽度
        lencity = utf8_strlen((const char*)config.city);
        lenwea = utf8_strlen(weather.c_str());

        lenwea = (75- (lencity +lenwea)* 12)/2;

        u8g2.setCursor(0, 60);
        u8g2.print(config.city);

        u8g2.setCursor(lenwea+lencity*12, 60);
        u8g2.print(weather);

        // u8g2.drawRBox(75, 64-15, 128-75, 15, 3); // (x,y,w,h,r)

        // u8g2.setFont(u8g_font_profont15);
        u8g2.setDrawColor(1);
        sprintf(dateStr, "%s%%", humidity);
        u8g2.setCursor(75, 60);
        u8g2.print(dateStr);
        u8g2.setCursor(102, 60);
        sprintf(dateStr, "%s°C", temperature);
        // sprintf(dateStr, "%sC", temperature);
        u8g2.print(dateStr);

        u8g2.setDrawColor(1);
        break;
      // 显示警告
      case 1:
      case 2:
        u8g2.setCursor(0, 60);
        u8g2.setFont(u8g2_font_wqy12_t_gb2312);


        utf8_substr(scrollText.c_str(), scrollIndex, 20, buff, sizeof(buff));

        u8g2.setCursor(0, 60);
        u8g2.print(buff);
        break;
      // // 显示警告
      // case 2:
      //   u8g2.setCursor(0, 60);
      //   u8g2.setFont(u8g2_font_wqy12_t_gb2312);

      //   utf8_substr(scrollText.c_str(), scrollIndex, 20, buff, sizeof(buff));

      //   u8g2.setCursor(0, 60);
      //   u8g2.print(buff);
      //   break;
      default:
        break;
    }
    
    u8g2.setCursor(4, 43);
    // u8g2.setFont(u8g_font_profont29);
    u8g2.setFont(u8g2_font_fur25_tf);
    
    u8g2.print(hm);

    u8g2.setCursor(90, 43);
    // u8g2.setFont(u8g_font_profont17);
    u8g2.setFont(u8g2_font_fur17_tf);
    u8g2.print(sec);

    u8g2.setDrawColor(1); // 1=正常色，2=XOR模式
    // // 绘制白色填充圆角矩形
    u8g2.drawRFrame(0, 14, 128, 34, 3); // (x,y,w,h,r)

  }

  while (u8g2.nextPage());
  scrollIndex += 1;
  
  if (scrollIndex >= utf8_strlen(scrollText.c_str()))
  {
    scrollIndex = 0;
  }
}


