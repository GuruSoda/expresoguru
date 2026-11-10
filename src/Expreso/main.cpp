#include <iostream>
#include <fstream>
#include <GuruSocket/GuruServidorTCP.hpp>
#include <GuruSocket/GuruServidorTCPProtocolo.hpp>
#include <GuruSocket/GuruProtocoloServidor.hpp>
#include <GuruSocket/GuruProtocoloCliente.hpp>
#include <GuruString_algo.hpp>
#include <GuruEtiquetaLeer.hpp>
#include <GuruBuffer.hpp>
#include <GuruError.hpp>
#include <GuruBase64.hpp>
#include <GuruSvc_algo.hpp>
#include <GuruDirectorio.hpp>
#include <GuruHConsola.hpp>

#include <jsoncpp/json.h>

#define EXPRESOGURU_VERSION_STRING "0.5.2"

class Expreso;

struct configuracionExpresoServer {
public:
	configuracionExpresoServer():habilitado(true) {};
	int		puerto;
	string	interfaz;
	bool	habilitado;
};

struct configuracionExpreso {
	string	proxy;
	int		puerto;
	string	usuario;
	string	clave;
	bool	cifrado;
	string	expresoServidor;
	string	interfaz;
};

struct conexion {
public:
	conexion():enchufe(NULL) {}
	int		puertoLocal;
	string	interfazRemota;
	int		puertoRemoto;

	GuruServidorTCP<Expreso>*	enchufe;
	configuracionExpreso		confExpreso;
};

struct ParametrosTunel{
public:
	ParametrosTunel():cifrado(false) {};
	GuruSocket	origen;
	GuruSocket	destino;
	bool		cifrado;
};

GuruSocket ConectarProxy(const string& proxy, const int puerto, const string& sitio, const string& usuario, const string& clave) {

		GuruSocket enchufe;

		enchufe = FabricaSocketConectar(proxy, puerto);

		GuruSocketTxt enchufetmp(enchufe);

		enchufetmp << "CONNECT " << sitio << " HTTP/1.1" << GuruSocketBase::crlf();
		if (!usuario.empty() && !clave.empty()) 
			enchufetmp << "Proxy-Authorization: Basic " << GuruBase64::encode(usuario + ":" + clave) << GuruSocketBase::crlf();
		enchufetmp << "Proxy-Connection: Keep-Alive" << GuruSocketBase::crlf();
		enchufetmp << "Host: " << sitio << GuruSocketBase::crlf();
		enchufetmp << GuruSocketBase::crlf();

		string buf; 
		do {
			enchufetmp >> buf; // HTTP/1.0 200 Connection established
			cout << buf << endl;
		} while (!buf.empty());

		return enchufe;
}

#ifdef __GURUWIN32__
static DWORD WINAPI Tunel(LPVOID parametros) {
#elif defined __GURULINUX__
static void* Tunel(void* parametros) {
#endif

	struct ParametrosTunel* p = static_cast<ParametrosTunel*>(parametros);

	int	cant_sockets = 0;
	int	leidos = 0;

	fd_set lectura;

	GuruBufferChar buf(static_cast<int>(BufferEntrada(p->origen.handle())));

	do {
		FD_ZERO(&lectura);

		FD_SET(p->origen.handle(), &lectura);

		cant_sockets = select((int)p->origen.handle() + 1, &lectura, NULL, NULL, NULL);

		// pregunto por time out o por error en el socket
		if (cant_sockets == 0) break;

		// pregunto por error en el socket
		 if (cant_sockets == SOCKET_ERROR) break; 

		leidos = recv (p->origen.handle(), buf.buf(), static_cast<int>(buf.cantidad_buf()), 0);

		if (leidos == 0) break; 

		if (leidos == SOCKET_ERROR) break; 

		buf.cantidad_data(leidos);

		int byte_enviados = send ((int)p->destino.handle(), buf.buf(), static_cast<int>(buf.cantidad_data()), 0);

		if (byte_enviados == SOCKET_ERROR) break;

	}while (8);

	#ifdef __GURUWIN32__
		ExitThread(0);
	#elif defined __GURULINUX__
		pthread_exit(0);
	#endif

	return 0;
};

void ArmarTunel(GuruSocket& origen, GuruSocket& destino) {
		struct ParametrosTunel a;
		a.origen = origen;
		a.destino = destino;

		struct ParametrosTunel b;
		b.origen = destino;
		b.destino = origen;

	#ifdef __GURUWIN32__
		HANDLE handle_thread_entrada = CreateThread(NULL, 0, Tunel, static_cast<void*>(&a), 0, NULL);
		if (handle_thread_entrada == NULL) throw GuruSocketServidorException();

		HANDLE handle_thread_salida = CreateThread(NULL, 0, Tunel, static_cast<void*>(&b), 0, NULL);
		if (handle_thread_salida == NULL) throw GuruSocketServidorException();

		HANDLE tuneles[2] = {handle_thread_entrada, handle_thread_salida};

		WaitForMultipleObjects(2, tuneles, FALSE, INFINITE);

		CloseHandle(handle_thread_entrada);
		CloseHandle(handle_thread_salida);
	#elif defined __GURULINUX__
/*
        signal(SIGHUP, SIG_IGN);
        signal(SIGINT, SIG_IGN);
        signal(SIGTERM, SIG_IGN);
        signal(SIGQUIT, SIG_IGN);
        signal(SIGPIPE, SIG_IGN);
*/
		pthread_t handle_thread_entrada,handle_thread_salida;
		if (pthread_create(&handle_thread_entrada, NULL, Tunel, static_cast<void*>(&a)) != 0)
			throw GuruSocketServidorException();

		if (pthread_create(&handle_thread_salida, NULL, Tunel, static_cast<void*>(&b)) != 0)
			throw GuruSocketServidorException();

		bool torigen,tdestino;
		do {
			Sleep(250);
			torigen = (pthread_kill(handle_thread_entrada,0) == ESRCH) ? false : true;
			tdestino = (pthread_kill(handle_thread_salida,0) == ESRCH) ? false : true;
		} while (torigen && tdestino);

	#endif
};

class Expreso: public GuruSocketServicio {
	GuruSocket destino;
public:
	Expreso () {};
	virtual ~Expreso() {destino.cerrar();};

	void main(void) {

		struct conexion* conex=static_cast<struct conexion*>(info()->extra_ptr());
		
		if (!conex->confExpreso.proxy.empty()) {
			string servidorRemoto;

			servidorRemoto = conex->confExpreso.expresoServidor.empty() ? (conex->interfazRemota + ":" + GuruStringAlgo::itos(conex->puertoRemoto)) : conex->confExpreso.expresoServidor;

			destino = ConectarProxy(conex->confExpreso.proxy, conex->confExpreso.puerto, servidorRemoto, conex->confExpreso.usuario, conex->confExpreso.clave);
		} else
			destino = FabricaSocketConectar(conex->interfazRemota, conex->puertoRemoto);

		if (!conex->confExpreso.expresoServidor.empty()) {
			GuruProtocoloCliente cliente(destino);
			ProtocoloRespuesta	resp;

			if (cliente.respuesta().tipo() != GuruProtocolo::codigo_bien()) return ;

			ostringstream oss;

			oss << "expreso servidor=" << conex->interfazRemota << " puerto=" << conex->puertoRemoto;

			if (cliente.cmd_linea(oss.str()).tipo() != GuruProtocolo::codigo_bien()) return ;
		}

		ArmarTunel(SocketEntrada(), destino);

		destino.cerrar();
	}
};

class GuruProtocoloExpreso : public GuruProtocoloServidor {
public:
	GuruProtocoloExpreso(IEnviar& enviar, IRecibir& recibir):GuruProtocoloServidor(enviar, recibir) {};
	virtual ~GuruProtocoloExpreso() {};

	bool init() {

		cargar_msg_bienvenida("Bienvenido al Expreso Guru.");
		cargar_msg_salida("Gracias por utilizar el Expreso Guru.");

		cargar_salida("salir");

		cargar_comando(this, &GuruProtocoloExpreso::cmd_expreso, "expreso", "Toma el Expreso Guru.");
		cargar_comando(this, &GuruProtocoloExpreso::cmd_compilacion, "compilacion", "Fecha de compilacion.");
		cargar_comando(this, &GuruProtocoloExpreso::cmd_version, "version", "Version del Expreso Guru.");

		return true;
	};

//	expreso servidor=192.168.1.21 puerto=3389
	void cmd_expreso(const vector<string>& parametros) {
		GuruEtiquetaLeerEx	tag(parametros);

		string servidor;
		::uint16_t puerto;

		try {
			servidor=tag["Servidor"];
			puerto=atoi(tag["Puerto"].c_str());

			if (servidor.empty()) throw string("Falta etiqueta Servidor.");
			if (puerto == 0) throw string("Falta etiqueta Puerto.");

			GuruSocket destino;

			destino = FabricaSocketConectar(servidor, puerto);

			BIEN();

			ArmarTunel(recibir.interfaz()->sacar_socket(), destino);

		} catch(const string& e) {
			MAL(e);
		} catch(const GuruSocketException& e) {
			MAL(e.what());
		}
	}

	void cmd_compilacion(const vector<string>& parametros) {
		BIEN(__DATE__ " - " __TIME__);
	}

	void cmd_version(const vector<string>& parametros) {
		BIEN(EXPRESOGURU_VERSION_STRING);
	}
};

void cargarConf(const string& archivo, configuracionExpreso& cliente, vector<conexion>& expreso, configuracionExpresoServer& server) {
	std::ifstream	archivoconf;
	Json::Value		conf;
	Json::Reader	lector;

	expreso.clear();

	archivoconf.open(archivo.c_str(), std::ifstream::binary);

	if (!archivoconf) {
		cout << "Error abriendo " << archivo << ":" << GuruError().descripcion() << endl;
		return ;
	}

	if (!lector.parse(archivoconf, conf, false)) { std::cout << lector.getFormattedErrorMessages() << endl; return ;}

////////////[Seccion Servidor]////////////////////////////////////////
	Json::Value valServidor(conf["Servidor"]);

	if (!valServidor.empty()) {

		server.puerto = valServidor["Puerto"].asInt();
		server.interfaz = valServidor["Interfaz"].empty() ? string("0.0.0.0") : valServidor["Interfaz"].asString();

		server.habilitado = (server.puerto != 0) ? true : false;

		if (!valServidor["Habilitado"].empty())
			server.habilitado = (valServidor["Habilitado"].asString().compare("no") == 0) ? false : true;
		else
			server.habilitado = true;

	} else 
		server.habilitado = false;

/////////////[Seccion Cliente]////////////////////////////////////////
	Json::Value valCliente(conf["Cliente"]);

	if (!valCliente.empty()) {
		cliente.proxy = valCliente["Proxy"].asString();
		cliente.puerto = valCliente["Puerto"].asInt();
		cliente.usuario = valCliente["Usuario"].asString();
		cliente.clave = valCliente["Clave"].asString();
		cliente.cifrado = (valCliente["Cifrado"].asString().compare("no") == 0) ? false : true;
		cliente.expresoServidor = valCliente["ExpresoServer"].asString();
		cliente.interfaz = valCliente["Interfaz"].empty() ? "127.0.0.1" : valCliente["Interfaz"].asString();
	}

/////////////[Seccion Expreso]////////////////////////////////////////
	Json::Value redi;
	redi = conf["Expreso"];

	if (!redi.empty()) {

		struct conexion conex;

		for (Json::ValueIterator it = redi.begin(); it != redi.end(); ++it) {
			Json::Value val(*it);

			conex.confExpreso = cliente;

			try {
				if ((conex.puertoLocal = val["PuertoLocal"].asInt())==0) throw string("No puede faltar PuertoLocal.");
				if ((conex.puertoRemoto = val["PuertoRemoto"].asInt())==0) throw string("No puede faltar PueroRemoto.");
				if ((conex.interfazRemota = val["InterfazRemota"].asString()).empty()) throw string("No puede faltar InterfazRemota.");

				// Para sobreescritura de los parametros globales ("Expreso")
				if (!val["Proxy"].empty()) {
					conex.confExpreso.proxy = val["Proxy"].asString();
					if (!val["Puerto"].empty() == 0) conex.confExpreso.puerto = val["Puerto"].asInt();

					if (!val["Usuario"].empty()) conex.confExpreso.usuario = val["Usuario"].asString();
					if (!val["Clave"].empty()) conex.confExpreso.clave = val["Clave"].asString();
				}

				if (!val["Cifrado"].empty()) conex.confExpreso.cifrado = (val["Cifrado"].asString().compare("no") == 0) ? false : true;
				if (!val["ExpresoServer"].empty())conex.confExpreso.expresoServidor = val["ExpresoServer"].asString();

				if (!val["InterfazLocal"].empty()) conex.confExpreso.interfaz = val["InterfazLocal"].asString();
				else conex.confExpreso.interfaz = "0.0.0.0";

				expreso.push_back(conex);
			} catch (string& e) {
				cout << e << endl;
			}
		}
	}

};

void ejemploConf () {
	Json::Value conf;

	Json::Value server;

	server["Interfaz"] = "0.0.0.0";
	server["Puerto"] = 443;
	server["Habilitado"] = "si";

	conf["Servidor"] = server;

	Json::Value seccion;

	seccion["Proxy"] = "10.1.1.86";
	seccion["Puerto"] = 3128;
	seccion["Usuario"] = "frossi";
	seccion["Clave"] = "alfa1234";
	seccion["Cifrado"] = "no";
	seccion["ExpresoServer"] = "www.planetaguru.com.ar:443";
	seccion["Interfaz"] = "127.0.0.1";

	conf["Cliente"] = seccion;

	seccion.clear();

	Json::Value redireccion;

	seccion["PuertoLocal"] = 22000;
	seccion["InterfazLocal"] = "127.0.0.1";
	seccion["PuertoRemoto"] = 22;
	seccion["InterfazRemota"] = "192.168.1.199";

	redireccion.append(seccion);

	seccion["PuertoLocal"] = 3690;
	seccion["PuertoRemoto"] = 3690;
	seccion["InterfazRemota"] = "192.168.1.2";

	redireccion.append(seccion);

	conf["Expreso"] = redireccion;

	Json::StyledWriter writer;
	cout << writer.write(conf) << endl;
};

class ExpresoSvc: public GuruSvc
{
	vector<conexion>		expreso;
	configuracionExpreso	confExpreso;
	configuracionExpresoServer server;

public:
	ExpresoSvc(const string& nombre_interno, const string& nombre_externo, bool MsgOnDll = false):GuruSvc(nombre_interno, nombre_externo, MsgOnDll) {};
	virtual ~ExpresoSvc(void) {};

	bool Inicializo() {
		cargarConf("Expreso.conf",confExpreso, expreso, server);

		for (vector<conexion>::iterator it=expreso.begin();it!=expreso.end();++it) {
			it->enchufe = new GuruServidorTCP<Expreso>(it->puertoLocal, it->confExpreso.interfaz);

			it->enchufe->extra_ptr(&(*it));
		}

		return true;
	}

	void Start() {

		ios_base::sync_with_stdio();

		GuruServidorTCPProtocolo<GuruProtocoloExpreso>	p(server.puerto, server.interfaz);

		try{		

			try {
				if (server.habilitado) p.iniciar();
			} catch (GuruSocketServidorException& e) {
				cout << "Expreso Server:" << e.what() << endl;
			}

			try {
				for (vector<conexion>::iterator it=expreso.begin();it!=expreso.end();++it)
					it->enchufe->iniciar();
			} catch (GuruSocketServidorException& e) {
				cout << "Redireccion:" << e.what() << endl;
			}

			EsperarFinalizacion();
		}catch(std::exception&) {
		}

		for (vector<conexion>::iterator it=expreso.begin();it!=expreso.end();++it) {
			it->enchufe->detener();
			delete it->enchufe;
		}

		p.detener();
	}
};

void cambiar_directorio_apps(const char* ruta)
{
	GuruDirectorio::GuruSplitPath	ruta_svc(ruta);

	GuruDirectorio::cambiarDirectorio(string(ruta_svc.drive() + ruta_svc.dir()));
}

int main (int cant, char* parametros[])
{
//	ejemploConf(); return 0;

	inicializar_GuruSocket();

	cambiar_directorio_apps(parametros[0]);

	GuruHConsola guruout;

	guruout.redirect_cout("system.out");
	guruout.redirect_cerr("system.err");

	GuruSvcAlgo::iniciar_servicio<ExpresoSvc>("ExpresoGuru","Expreso Guru", cant, parametros);

	guruout.redirect_cout();
	guruout.redirect_cerr();

	limpiar_GuruSocket();

	return 0;
}
