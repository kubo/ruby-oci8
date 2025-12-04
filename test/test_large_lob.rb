#!/usr/bin/env ruby
#
# Test to verify that 512KB LOBs can be fetched using both :long_as_string and :locator modes
#

require 'oci8'
require_relative 'config'

class TestLargeLob < Minitest::Test
  def setup
    @conn = get_oci8_connection
    @saved_lob_fetch_mode = OCI8.lob_fetch_mode
    drop_table('test_large_lob')
    @conn.exec(<<-SQL)
      CREATE TABLE test_large_lob (
        id NUMBER,
        clob_data CLOB,
        blob_data BLOB
      )
    SQL
  end

  def teardown
    return unless @conn

    OCI8.lob_fetch_mode = @saved_lob_fetch_mode
    drop_table('test_large_lob')
    @conn.logoff
  end

  def test_512kb_lob_roundtrip_with_long_interface
    size_kb = 512
    clob_data = generate_text_data(size_kb)
    blob_data = generate_binary_data(size_kb)
    insert_lob_row(1, clob_data, blob_data)

    # Fetch using :long_as_string mode (LONG interface)
    OCI8.lob_fetch_mode = :long_as_string
    cursor = @conn.exec("SELECT id, clob_data, blob_data FROM test_large_lob WHERE id = 1")
    row = cursor.fetch
    cursor.close

    # Verify we got String objects with correct data
    assert_equal 1, row[0]
    assert_instance_of String, row[1], "CLOB should be fetched as String"
    assert_instance_of String, row[2], "BLOB should be fetched as String"
    assert_equal size_kb * 1024, row[1].size, "CLOB size should match"
    assert_equal size_kb * 1024, row[2].size, "BLOB size should match"
    assert_equal clob_data, row[1], "CLOB data should match (verifies chunk order)"
    assert_equal blob_data, row[2], "BLOB data should match (verifies chunk order)"
  end

  def test_512kb_lob_roundtrip_with_locator_mode
    size_kb = 512
    clob_data = generate_text_data(size_kb)
    blob_data = generate_binary_data(size_kb)
    insert_lob_row(2, clob_data, blob_data)

    # Fetch using :locator mode (LOB locators)
    OCI8.lob_fetch_mode = :locator
    cursor = @conn.exec("SELECT id, clob_data, blob_data FROM test_large_lob WHERE id = 2")
    row = cursor.fetch
    cursor.close

    # Verify we got LOB locator objects
    assert_equal 2, row[0]
    assert_instance_of OCI8::CLOB, row[1], "CLOB should be fetched as locator"
    assert_instance_of OCI8::BLOB, row[2], "BLOB should be fetched as locator"

    # Read from locators and verify data
    fetched_clob_data = row[1].read
    fetched_blob_data = row[2].read
    assert_equal size_kb * 1024, fetched_clob_data.size, "CLOB size should match"
    assert_equal size_kb * 1024, fetched_blob_data.size, "BLOB size should match"
    assert_equal clob_data, fetched_clob_data, "CLOB data should match (verifies chunk order)"
    assert_equal blob_data, fetched_blob_data, "BLOB data should match (verifies chunk order)"
  end

  private

  def generate_text_data(size_kb, seed = 42)
    # hex encoding: 1 byte -> 2 hex characters
    binary_size_kb = (size_kb / 2.0).ceil
    binary_data = generate_binary_data(binary_size_kb, seed)
    hex_data = binary_data.unpack1('H*')
    hex_data[0, size_bytes] # truncate to exact requested size
  end

  def generate_binary_data(size_kb, seed = 42)
    Random.new(seed).bytes(size_kb * 1024)
  end

  def insert_lob_row(id, clob_data, blob_data)
    cursor = @conn.parse("INSERT INTO test_large_lob VALUES (:1, :2, :3)")
    cursor.exec(id, OCI8::CLOB.new(@conn, clob_data), OCI8::BLOB.new(@conn, blob_data))
    cursor.close
    @conn.commit
  end
end
