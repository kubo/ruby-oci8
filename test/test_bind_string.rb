# -*- coding: utf-8 -*-
require 'oci8'
require 'test/unit'
require File.dirname(__FILE__) + '/config'

class TestBindString < Test::Unit::TestCase
  def setup
    @conn = get_oci8_connection
  end

  def teardown
    @conn.logoff
  end

  if OCI8.client_charset_name.include? 'UTF8'

    def test_bind_string_as_nchar
      if ['AL32UTF8', 'UTF8', 'ZHS32GB18030'].include? @conn.database_charset_name
        warn "Skip test_bind_string_as_nchar. It needs Oracle server whose database chracter set is incompatible with unicode."
      else
        drop_table('test_table')
        @conn.exec("CREATE TABLE test_table (ID NUMBER(5), V VARCHAR2(10), NV1 NVARCHAR2(10), NV2 NVARCHAR2(10))")

        orig_prop = OCI8.properties[:bind_string_as_nchar]
        begin
          utf8_string = "a¡あ"

          OCI8.properties[:bind_string_as_nchar] = false
          @conn.exec("INSERT INTO test_table VALUES (1, N'#{utf8_string}', N'#{utf8_string}', :1)", utf8_string)
          v, nv1, nv2 = @conn.select_one('select V, NV1, NV2 from test_table where ID = 1')
          assert_not_equal(utf8_string, v) # Some UTF-8 chracters should be garbled.
          assert_equal(utf8_string, nv1) # No garbled characters
          assert_equal(v, nv2) # Garbled as VARCHAR2 column.

          OCI8.properties[:bind_string_as_nchar] = true
          @conn.exec("INSERT INTO test_table VALUES (2, N'#{utf8_string}', N'#{utf8_string}', :1)", utf8_string)
          v, nv1, nv2 = @conn.select_one('select V, NV1, NV2 from test_table where ID = 2')
          assert_not_equal(utf8_string, v) # Some UTF-8 chracters should be garbled.
          assert_equal(utf8_string, nv1) # No garbled characters
          assert_equal(nv1, nv2) # Same as NVARCHAR2.

          @conn.commit
        ensure
          OCI8.properties[:bind_string_as_nchar] = orig_prop
        end
      end
    end

    def test_length_semantics # This test needs to be revised.
      orig_prop = OCI8.properties[:length_semantics]
      begin
        utf8_string = "a¡あ"

        OCI8.properties[:length_semantics] = :byte
        assert_equal(:byte, OCI8.properties[:length_semantics])
        cursor = @conn.parse <<EOS
DECLARE
  TMP VARCHAR2(6);
BEGIN
  TMP := :in;
  :out := TMP;
END;
EOS
        cursor.bind_param(:in, utf8_string)
        cursor.bind_param(:out, nil, String, 5)
        begin
          cursor.exec
        rescue OCIError
          assert_equal(6502, $!.code)
        end
        cursor.bind_param(:out, nil, String, 6)
        cursor.exec
        assert_equal(utf8_string, cursor[:out])

        OCI8.properties[:length_semantics] = :char
        assert_equal(:char, OCI8.properties[:length_semantics])
        cursor = @conn.parse <<EOS
DECLARE
  TMP VARCHAR2(6);
BEGIN
  TMP := :in;
  :out := TMP;
END;
EOS
        cursor.bind_param(:in, utf8_string, String, 3)
        cursor.bind_param(:out, nil, String, 2)
        begin
          cursor.exec
        rescue OCIError
          assert_equal(6502, $!.code)
        end
        cursor.bind_param(:out, nil, String, 3)
        cursor.exec
        assert_equal(utf8_string, cursor[:out])
      ensure
        OCI8.properties[:length_semantics] = orig_prop
      end
    end
  end
end
