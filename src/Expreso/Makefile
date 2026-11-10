.SUFFIXES = .c .cpp

BOOST=../../../boost
LIBRERIAS=../../../Librerias
DIR_OBJETOS = ../build/linux/

SALIDA_EJECUTABLE=../../bin
MIS_LINKS=-lpthread -ldl
MIS_DEFINE=

# Para compilar en 64 sobre 32, instalar el paquete g++-multilib
ifeq ($(BITS),64)
    CXXFLAGS+=-m64
else ifeq ($(BITS),32)
        CXXFLAGS+=-m32
endif

ifeq (${DEBUG},1)
 CXXFLAGS+=-g -rdynamic
else
 CXXFLAGS+=-s -O2
endif

# mismos parametros para compilar C/C++
# CFLAGS=$(CXXFLAGS)

RAIZ=.
INCLUDED=${LIBRERIAS}/ \
	${LIBRERIAS}/GuruSocket/ \
	${LIBRERIAS}/polarssl/include/ \
	${LIBRERIAS}/polarssl/library/ \
	${LIBRERIAS}/jsoncpp/ \
	${LIBRERIAS}/GuruCompat/

MIS_LIBS := xtoa.o jsoncpp.o base64.o GuruSvc.o 
OBJETOS += $(addprefix ${DIR_OBJETOS}, ${MIS_LIBS})

PROGRAMA := main.o
OBJETOS += $(addprefix ${DIR_OBJETOS}, ${PROGRAMA})

PARAM_INCLUDE:=$(addprefix "-I", ${INCLUDED})
VPATH=${INCLUDED}

CXXFLAGS=-std=c++11

expreso: crear_directorios Makefile ${OBJETOS}
	@$(CXX) ${CXXFLAGS} $(LDFLAGS) -o $(SALIDA_EJECUTABLE)/$@ $(OBJETOS) ${MIS_LINKS}
	@echo "Todo terminado????"

${DIR_OBJETOS}%.o: %.cpp
	@echo "Compilando C++ $? -> $@"
	@${CXX} -O2 -x c++ -pipe ${CXXFLAGS} ${MIS_DEFINE} ${PARAM_INCLUDE} -c $? -o $@

${DIR_OBJETOS}%.o:%.c
	@echo "Compilando C $? -> $@"
	@${CC} -O2 -s -pipe ${CFLAGS} ${PARAM_INCLUDE} ${MIS_DEFINE} -c $? -o $@

clean:
	@rm -f ${OBJETOS}

crear_directorios:
	@mkdir -p ${DIR_OBJETOS}

compilar:
	@echo ${VPATH} ${OBJETOS}

envdeb:
	@$(eval DST:=/usr/local)

envopt: 
	@$(eval DST:=/opt/atlas)

makedeb: expreso 
	@$(eval DIR_TEMP:=$(shell mktemp -du --suffix=atlas))
	@$(eval RELEASE:=$(shell date +%Y%m%d.%H%M))
	@$(eval VERSION:=$(shell cat debian/control | grep Version | cut -d ':' -f 2 | tr -d '[:blank:]'))
	@mkdir -p ${DIR_TEMP}${DST}/bin
	@mkdir -p ${DIR_TEMP}${DST}/sitios
	@mkdir -p ${DIR_TEMP}${DST}/log
	@mkdir -p ${DIR_TEMP}/DEBIAN
	@mkdir -p ${DIR_TEMP}/etc/init.d
	@cp ../../bin/atlas ${DIR_TEMP}${DST}/bin
	@cp ../../bin/atlas.css ${DIR_TEMP}${DST}/bin
	@cp ../../bin/atlas.mime ${DIR_TEMP}${DST}/bin
	@cp ../../bin/atlas.init ${DIR_TEMP}/etc/init.d/atlas
	@sed 's/^Version:\ [0-9]\..*/Version: ${VERSION}-${RELEASE}/' debian/control > ${DIR_TEMP}/DEBIAN/control 
	@dpkg -b ${DIR_TEMP} ../../atlas-${VERSION}-${RELEASE}_i386.deb
	@rm -rf ${DIR_TEMP}

deb: envdeb makedeb
	@echo "Todo Listo."

debopt: envopt makedeb
	@echo "Todo Listo."

