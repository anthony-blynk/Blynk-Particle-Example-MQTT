#include "unity.h"

#include "JsonWriter.h"

char buff[256];

void test_utf8() {
  JsonBufferWriter writer(buff, sizeof(buff));
  writer.beginObject();
    writer["морда"] = "😁";
  writer.endObject();

  TEST_ASSERT_EQUAL_INT(21, writer.dataSize());
  TEST_ASSERT_EQUAL_MEMORY(R"json({"морда":"😁"})json",
                           writer.buffer(), writer.dataSize());
}

void test_multiline() {
  JsonBufferWriter writer(buff, sizeof(buff));
  writer.beginObject();
    writer["спец"]  = "\"привіт\nсвіт\"\t\n😁\\\\\\";
  writer.endObject();

  TEST_ASSERT_EQUAL_INT(55, writer.dataSize());
  TEST_ASSERT_EQUAL_MEMORY(R"json({"спец":"\"привіт\nсвіт\"\t\n😁\\\\\\"})json",
                           writer.buffer(), writer.dataSize());
}

void test_ascii_only() {
  JsonBufferWriter writer(buff, sizeof(buff));
  writer.setAsciiOnly();
  writer.beginObject();
    writer["ascii"]  = "тут ASCII тільки 😁";
  writer.endObject();

  TEST_ASSERT_EQUAL_INT(42, writer.dataSize());
  TEST_ASSERT_EQUAL_MEMORY(R"json({"ascii":"тут ASCII тільки 😁"})json",
                           writer.buffer(), writer.dataSize());
}

int runUnityTests(void) {
  UNITY_BEGIN();
  RUN_TEST(test_utf8);
  RUN_TEST(test_multiline);
  RUN_TEST(test_ascii_only);
  return UNITY_END();
}


void setup() {
  delay(1000);

  runUnityTests();
}

void loop() {
}
