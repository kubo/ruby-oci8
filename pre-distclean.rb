rm_f "#{curr_objdir}/lib/oci8.rb"
if RUBY_PLATFORM =~ /cygwin/
  rm_f "#{curr_objdir}/ext/oci8/OCI.def"
  rm_f "#{curr_objdir}/ext/oci8/libOCI.a"
end
