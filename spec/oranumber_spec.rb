require 'bigdecimal'
require 'rational'
require File.join(File.dirname(__FILE__), 'spec_helper.rb')

describe OraNumber do

  it "should be converted to Flaot by to_f" do
    OraNumber("0.25").to_f.should eql(0.25)
  end

  it "should be converted to Integer by to_i" do
    OraNumber("7.25").to_i.should eql(7)
    OraNumber("7.75").to_i.should eql(7)
  end

  it "should be converted to Rational by to_r" do
    OraNumber("100000000000000.1").to_r.should eql(Rational(1000000000000001, 10))
  end

  it "should be converted to BigDecimal by to_d" do
    OraNumber("100000000000000.1").to_d.should eql(BigDecimal("100000000000000.1"))
  end

  # OraNumber bin_op OraNumber
  it "+ OraNumber should be an OraNumber" do
    (OraNumber(2) + OraNumber(1)).should eql(OraNumber(3))
  end

  it "- OraNumber should be an OraNumber" do
    (OraNumber(2) - OraNumber(1)).should eql(OraNumber(1))
  end

  it "* OraNumber should be an OraNumber" do
    (OraNumber(2) * OraNumber(1)).should eql(OraNumber(2))
  end

  it "/ OraNumber should be an OraNumber" do
    (OraNumber(2) / OraNumber(1)).should eql(OraNumber(2))
  end

  # OraNumber bin_op Fixnum
  it "+ Fixnum should be an OraNumber" do
    (OraNumber(2) + 1).should eql(OraNumber(3))
  end

  it "- Fixnum should be an OraNumber" do
    (OraNumber(2) - 1).should eql(OraNumber(1))
  end

  it "* Fixnum should be an OraNumber" do
    (OraNumber(2) * 1).should eql(OraNumber(2))
  end

  it "/ Fixnum should be an OraNumber" do
    (OraNumber(2) / 1).should eql(OraNumber(2))
  end

  # OraNumber bin_op Bignum
  it "+ Bignum should be an OraNumber" do
    (OraNumber(2) + 100000000000000000000).should eql(OraNumber(100000000000000000002))
  end

  it "- Bignum should be an OraNumber" do
    (OraNumber(2) - 100000000000000000000).should eql(OraNumber(-99999999999999999998))
  end

  it "* Bignum should be an OraNumber" do
    (OraNumber(2) * 100000000000000000000).should eql(OraNumber(200000000000000000000))
  end

  it "/ Bignum should be an OraNumber" do
    (OraNumber(2) / 100000000000000000000).should eql(OraNumber("0.00000000000000000002"))
  end

  # OraNumber bin_op Float
  it "+ Float should be a Float" do
    (OraNumber(2) + 1.0).should eql(3.0)
  end

  it "- Float should be a Float" do
    (OraNumber(2) - 1.0).should eql(1.0)
  end

  it "* Float should be a Float" do
    (OraNumber(2) * 1.0).should eql(2.0)
  end

  it "/ Float should be a Float" do
    (OraNumber(2) / 1.0).should eql(2.0)
  end

  # OraNumber bin_op BigDecimal
  it "+ BigDecimal should be a BigDecimal" do
    (OraNumber(2) + BigDecimal("1")).should eql(BigDecimal("3"))
  end

  it "- BigDecimal should be a BigDecimal" do
    (OraNumber(2) - BigDecimal("1")).should eql(BigDecimal("1"))
  end

  it "* BigDecimal should be a BigDecimal" do
    (OraNumber(2) * BigDecimal("1")).should eql(BigDecimal("2"))
  end

  it "/ BigDecimal should be a BigDecimal" do
    (OraNumber(2) / BigDecimal("1")).should eql(BigDecimal("2"))
  end

  # OraNumber bin_op Rational
  it "+ Rational should be a Rational" do
    (OraNumber(2) + Rational(1)).should eql(Rational(3))
  end

  it "- Rational should be a Rational" do
    (OraNumber(2) - Rational(1)).should eql(Rational(1))
  end

  it "* Rational should be a Rational" do
    (OraNumber(2) * Rational(1)).should eql(Rational(2))
  end

  it "/ Rational should be a Rational" do
    (OraNumber(2) / Rational(1)).should eql(Rational(2))
  end
end

describe Fixnum do
  it "+ OraNumber should be an OraNumber" do
    (2 + OraNumber(1)).should eql(OraNumber(3))
  end

  it "- OraNumber should be an OraNumber" do
    (2 - OraNumber(1)).should eql(OraNumber(1))
  end

  it "* OraNumber should be an OraNumber" do
    (2 * OraNumber(1)).should eql(OraNumber(2))
  end

  it "/ OraNumber should be an OraNumber" do
    (2 / OraNumber(1)).should eql(OraNumber(2))
  end
end

describe Bignum do
  it "+ OraNumber should be an OraNumber" do
    (100000000000000000000 + OraNumber(1)).should eql(OraNumber(100000000000000000001))
  end
end

describe Float do
  it "+ OraNumber should be a Float" do
    (1.0 + OraNumber(1)).should eql(2.0)
  end
end

describe BigDecimal do
  it "+ OraNumber should be a BigDecimal" do
    (BigDecimal("1") + OraNumber(1)).should eql(BigDecimal("2"))
  end
end

describe Rational do
  it "+ OraNumber should be a Rational" do
    (Rational(1) + OraNumber(1)).should eql(Rational(2))
  end
end
