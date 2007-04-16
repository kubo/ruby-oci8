%define ruby_sitelib %(ruby -rrbconfig -e 'puts Config::CONFIG["sitelibdir"]')

Summary: ruby interface for Oracle using OCI8 API
Name: ruby-oci8
Version: 1.0.0_rc1
%define RealVersion 1.0.0-rc1
Release: 1%{?dist}
Group: Development/Libraries
License: Ruby License
URL: http://rubyforge.org/projects/ruby-oci8/
Source0: http://rubyforge.org/frs/download.php/16630/%{name}-%{RealVersion}.tar.gz
BuildRoot: %{_tmppath}/%{name}-%{version}-root-%(%{__id_u} -n)
Requires: ruby
Requires: oracle-instantclient-basic
BuildRequires: ruby
BuildRequires: ruby-devel
BuildRequires: oracle-instantclient-basic
BuildRequires: oracle-instantclient-devel

# Note: the oracle-instanclient-basic package should say
#   Provides: libclntsh.so.10.1
# but it doesn't. So we have to turn off dependency checking in the
# finished package. (Or else we could manually patch the package
# using rpmrebuild)
AutoReqProv: no

%description
Ruby/OCI8 is a ruby interface for Oracle using OCI8 API. It also provides
a DBD driver for use with ruby-dbi, and is used by the ActiveRecord Oracle
connection adapter.

%prep
%setup -q -n %{name}-%{RealVersion}

%build
CFLAGS="$RPM_OPT_FLAGS" \
LD_LIBRARY_PATH="$(echo /usr/lib/oracle/*/client/lib | sed 's/^.* //')" \
ruby setup.rb config -- --with-instant-client
make

%install
rm -rf $RPM_BUILD_ROOT
ruby setup.rb --verbose install --prefix=$RPM_BUILD_ROOT

%clean
rm -rf $RPM_BUILD_ROOT

%files
%defattr(-, root, root, -)
%doc ChangeLog NEWS README
%doc doc/api.en.html doc/manual.css

%{ruby_sitelib}/oci8.rb
%{ruby_sitelib}/DBD/OCI8/OCI8.rb
%{ruby_sitelib}/i386-linux/oci8lib.so

%changelog
* Fri Mar 23 2007 Brian Candler <brian.candler@inspiredbroadcast.net> - 1.0.0_rc1-1
- First cut
