# Low-level API
require 'oci8'
require 'test/unit'
require './config'

class TestBindRaw < Test::Unit::TestCase
  CHECK_TARGET = [
    ["0123456789:;<=>?", "303132333435363738393A3B3C3D3E3F"],
    ["@ABCDEFGHIJKLMNO", "404142434445464748494A4B4C4D4E4F"],
    ["PQRSTUVWXYZ[\\]^_", "505152535455565758595A5B5C5D5E5F"],
    ["`abcdefghijklmno", "606162636465666768696A6B6C6D6E6F"],
    ["pqrstuvwxyz{|}~", "707172737475767778797A7B7C7D7E"],
  ]

  def setup
    @env, @svc, @stmt = setup_lowapi()
  end

  def test_set_raw
    @stmt.prepare("BEGIN :hex := RAWTOHEX(:raw); END;")
    raw_in = @stmt.bindByName(":raw", OCI_TYPECODE_RAW, 16)
    hex_out = @stmt.bindByName(":hex", String, 32)
    
    CHECK_TARGET.each do |raw, hex|
      raw_in.set(raw)
      @stmt.execute(@svc)
      assert_equal(hex, hex_out.get())
    end
  end

  def test_get_raw
    @stmt.prepare("BEGIN :raw := HEXTORAW(:hex); END;")
    hex_in = @stmt.bindByName(":hex", String, 32)
    raw_out = @stmt.bindByName(":raw", OCI_TYPECODE_RAW, 16)
    
    CHECK_TARGET.each do |raw, hex|
      hex_in.set(hex)
      @stmt.execute(@svc)
      assert_equal(raw, raw_out.get())
    end
  end

  def teardown
    @stmt.free()
    @svc.logoff()
    @env.free()
  end
end
