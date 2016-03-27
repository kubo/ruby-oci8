if caller[0] !~ /\/bundler\/runtime\.rb:\d+:in `require'/
  warn "Don't requie 'ruby-oci8'. Use \"require 'oci8'\" instead. 'ruby-oci8.rb' was added only for 'Bundler.require'."
end
require 'oci8'
