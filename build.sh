mkdir sqlsmith_build
cd sqlsmith_build
set -x
if which yum; then
    yum install -y autoconf autoconf-archive automake gcc-c++ boost-devel postgresql-devel mysql-devel>/dev/null
    if grep '\(Red Hat\|ROSA\) Enterprise Linux Server release 6' \
        /etc/redhat-release; then
        wget -qO- http://people.redhat.com/bkabrda/scl_python27.repo >> \
            /etc/yum.repos.d/scl.repo
        yum install -y python27
        yum install -y \
            http://mirror.centos.org/centos/6/sclo/x86_64/rh/devtoolset-3/\
devtoolset-3-gcc-c++-4.9.2-6.2.el6.x86_64.rpm \
            http://mirror.centos.org/centos/6/sclo/x86_64/rh/devtoolset-3/\
devtoolset-3-gcc-4.9.2-6.2.el6.x86_64.rpm \
            http://mirror.centos.org/centos/6/sclo/x86_64/rh/devtoolset-3/\
devtoolset-3-runtime-3.1-12.el6.x86_64.rpm \
            http://mirror.centos.org/centos/6/sclo/x86_64/rh/devtoolset-3/\
devtoolset-3-libstdc++-devel-4.9.2-6.2.el6.x86_64.rpm \
            http://mirror.centos.org/centos/6/sclo/x86_64/rh/devtoolset-3/\
devtoolset-3-binutils-2.24-18.el6.x86_64.rpm
        source /opt/rh/devtoolset-3/enable
        source /opt/rh/python27/enable
        yum install -y http://mirrors.isu.net.sa/pub/fedora/fedora-epel/\
6/x86_64/autoconf-archive-2012.09.08-1.el6.noarch.rpm
    fi
    if grep '\(Red Hat\|ROSA\) Enterprise Linux \(Server\|Cobalt\) release 7'\
     /etc/redhat-release; then
        yum install -y http://mirror.centos.org/centos/\
7/os/x86_64/Packages/autoconf-archive-2017.03.21-1.el7.noarch.rpm
    fi
fi
export PATH=$1/bin:$PATH
curl --tlsv1.2 -sS -L https://github.com/jtv/libpqxx/archive/6.1.0.tar.gz \
    -o libpqxx.tar.gz || \
wget https://github.com/jtv/libpqxx/archive/6.1.0.tar.gz -O libpqxx.tar.gz
tar fax libpqxx.tar.gz
cd libpqxx*
./configure --disable-documentation && make && make install
cd ..
cd ..
touch gitrev.h
echo '#define GITREV "unreleased"' >> gitrev.h
sed -i -e 's/\$(PQXX_CFLAGS)/\$(LIBPQXX_CFLAGS)/' Makefile.am
autoreconf -i
sed -i -e 's/LIBS="-lmysqlclient/LIBS="-L\/usr\/lib64\/mysql -lmysqlclient/' configure
sed -i -e 's|/\* re-throw to outer loop to recover session. \*/|return 1;|' sqlsmith.cc
PKG_CONFIG_PATH=/usr/local/lib/pkgconfig/:$1/lib/pkgconfig/ \
LIBPQXX_LIBS="-L$1/lib -lpqxx -lpq" ./configure $CONF_OPTIONS && make -j4
