require File.join(File.dirname(__FILE__), 'spec_helper.rb')

describe OCI8::Object::Base, "when creating an instance" do
  before :all do
    @conn = get_oracle_connection()
    @conn.exec <<EOS
CREATE OR REPLACE TYPE rboci8_test_object AS OBJECT (
   char_attr char(5),
   varchar2_attr varchar2(5),
   raw_attr raw(5),
   number_attr number,
   integer_attr integer,
   float_attr double precision,
   date_attr date
);
EOS
  end

  after :all do
    @conn.exec "DROP TYPE rboci8_test_object"
    @conn.logoff
  end

  class Rboci8TestObject < OCI8::Object::Base
  end

  def make_attr(seed)
    [format("%5d", seed * 10),       # char_attr
     format("%d", seed * 11),     # varchar2_attr
     format("%010x", seed * 12),       # raw_attr
     OraNumber.new(seed * 13+ 0.5), # number_attr
     (seed * 999).to_i,             # integer_attr
     (seed * 1023).to_f + 0.25,     # float_attr
     Time.at(946702800 + seed * 10000).to_datetime, # date_attr
    ]
  end

  def check_attr(obj, attr)
    obj.char_attr    .should eql attr[0]
    obj.varchar2_attr.should eql attr[1]
    obj.raw_attr     .should eql attr[2] ? [attr[2]].pack('H*') : nil
    obj.number_attr  .should eql attr[3]
    obj.integer_attr .should eql attr[4]
    obj.float_attr   .should eql attr[5]
    obj.date_attr    .should eql attr[6]
  end

  it "should accept nil attributes" do
    0.upto 6 do |idx|
      attr = make_attr(idx)
      attr[idx] = nil
      obj = Rboci8TestObject.new(@conn, *attr)
      check_attr(obj, attr)
    end
  end
end
