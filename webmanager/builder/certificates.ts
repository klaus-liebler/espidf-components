import * as forge from 'node-forge';
import * as fs from 'fs';
import * as crypto from "node:crypto";

const rootCaExtensions = [{
	name: 'basicConstraints',
	cA: true
}, {
	name: 'keyUsage',
	keyCertSign: true,
	cRLSign: true
}];

function createSubject(commonName: string):forge.pki.CertificateField[] {
	return [{
		shortName: 'C',
		value: 'DE'
	}, {
		shortName: 'ST',
		value: 'NRW'
	}, {
		shortName: 'L',
		value: 'Greven'
	}, {
		shortName: 'O',
		value: 'Klaus Liebler'//hier muss vermutlich was stehen
	}, {
		shortName: 'OU',
		value: 'none'//hier muss vermutlich was stehen
	}, {
		shortName: 'CN',
		value: commonName//hier muss vermutlich beim host certificate etwas anderes stehen, als der Hostname
	}];
}

function createHostExtensions(dnsHostname:string, authorityKeyIdentifier:string){
	return [{
		name: 'basicConstraints',
		cA: false
	}, {
		name: 'nsCertType',
		server: true
	}, {
		name: 'subjectKeyIdentifier'
	}, {
		name: 'authorityKeyIdentifier',
		authorityCertIssuer: true,
		serialNumber: authorityKeyIdentifier
	}, {
		name: 'keyUsage',
		digitalSignature: true,
		nonRepudiation: true,
		keyEncipherment: true
	}, {
		name: 'extKeyUsage',
		serverAuth: true
	}, {
		name: 'subjectAltName',
		altNames: [{
			type: 2, // 2 is DNS type
			value: dnsHostname
		}]
	}];
}

// a hexString is considered negative if it's most significant bit is 1
// because serial numbers use ones' complement notation
// this RFC in section 4.1.2.2 requires serial numbers to be positive
// http://www.ietf.org/rfc/rfc5280.txt
function randomSerialNumber(numberOfBytes: number) {
	let buf = crypto.randomBytes(numberOfBytes);
	buf[0] = buf[0] & 0x7F;
	return buf.toString("hex");
}

function DateNDaysInFuture(n: number) {
	var d = new Date();
	d.setDate(d.getDate() + n);
	return d;
}

function certHelper(setPrivateKeyInCertificate:boolean, subject:forge.pki.CertificateField[], issuer:forge.pki.CertificateField[], exts: any[], signWith:forge.pki.PrivateKey|null){
	const keypair = forge.pki.rsa.generateKeyPair(2048);
	const cert = forge.pki.createCertificate();
	cert.publicKey = keypair.publicKey;
	if(setPrivateKeyInCertificate) cert.privateKey = keypair.privateKey;
	cert.serialNumber = randomSerialNumber(20);
	cert.validity.notBefore = new Date();
	cert.validity.notAfter = DateNDaysInFuture(100*365);//validity 100 years from now
	cert.setSubject(subject);
	cert.setIssuer(issuer);
	cert.setExtensions(exts);
	cert.sign(signWith??keypair.privateKey, forge.md.sha512.create());
	return { certificate: forge.pki.certificateToPem(cert), privateKey: forge.pki.privateKeyToPem(keypair.privateKey), };
}

export function CreateRootCA() {
	const subjectAndIssuer = createSubject("AAA Klaus Liebler Root CA");
	return certHelper(
		true,//necessary, found out in tests
		subjectAndIssuer,
		subjectAndIssuer, //self sign
		rootCaExtensions,
		null);//self sign
}

export function CreateCert(dnsHostname: string, certificateCaPemPath: fs.PathOrFileDescriptor, privateKeyCaPemPath: fs.PathOrFileDescriptor) {
	let caCert = forge.pki.certificateFromPem(fs.readFileSync(certificateCaPemPath).toString());
	let caPrivateKey = forge.pki.privateKeyFromPem(fs.readFileSync(privateKeyCaPemPath).toString());
	return certHelper(
		false, 
		createSubject("HTTPS Server on ESP32"), //CN of subject may not contain server address (found out by experiments)
		caCert.subject.attributes, //issuer is the subject of the rootCA
		createHostExtensions(dnsHostname, caCert.serialNumber),
		caPrivateKey //sign with private key of rootCA
	);
}

