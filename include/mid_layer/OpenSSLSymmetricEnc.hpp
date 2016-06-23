#pragma once
#include "SymmetricEnc.hpp"
#include <openssl/rand.h>
#include <openssl/evp.h>
#include "../primitives/Prf.hpp"

/**
* This is an abstract class that manage the common behavior of symmetric encryption using Open SSL library.
* We implemented symmetric encryption using OpenSSL with two modes of operations - CBC and CTR, each one has a unique derived class.
*
* @author Cryptography and Computer Security Research Group Department of Computer Science Bar-Ilan University (Moriya Farbstein)
*
*/
class OpenSSLEncWithIVAbs : public virtual SymmetricEnc {
protected:
	EVP_CIPHER_CTX *enc;							// A pointer to the native object that implements the encryption.
	EVP_CIPHER_CTX *dec;							// A pointer to the native object that implements the decryption.
	string prpName;					// The name of the underlying prp to use.

	void doConstruct();

	//Check that the given name is valid for this encryption scheme.
	virtual bool checkExistance(string prpName) = 0;

	// Return the size of the Iv in the current encryption scheme.
	int getIVSize() {	
		return EVP_CIPHER_CTX_iv_length(enc);
	}
	
	vector<byte> encryptOpenSSL(vector<byte> plaintext, vector<byte> iv);	//Encrypt the given plaintext.
	vector<byte> decryptOpenSSL(vector<byte> cipher, vector<byte> iv);		//Decrypt the given ciphertext.

																		
public:
	/**
	* Gets the name of the underlying prp that determines the type of encryption that will be performed.
	* A default source of randomness is used.
	* @param prp the underlying pseudorandom permutation to get the name of.
	*/
	OpenSSLEncWithIVAbs(PseudorandomPermutation* prp) : OpenSSLEncWithIVAbs(prp->getAlgorithmName()) {}

	/**
	* Sets the name of a Pseudorandom permutation and the source of randomness.<p>
	* The given prpName should be a name of prp algorithm such that OpenSSL provides an encryption with.
	* The following names are valid:
	* For CBC mode of operations: AES and TripleDES.
	* For CTR mode of operations: AES.
	* @param prpName the name of a specific Pseudorandom permutation, for example "AES".
	* @param random  a user provided source of randomness.
	* @throw IllegalArgumentException in case the given prpName is not valid for this encryption scheme.
	*/
	OpenSSLEncWithIVAbs(string prpName) { this->prpName = prpName; }

	virtual ~OpenSSLEncWithIVAbs();

	/**
	* Supply the encryption scheme with a Secret Key.
	*/
	void setKey(SecretKey secretKey) override;	

	/**
	* This function should not be used to generate a key for the encryption and it throws UnsupportedOperationException.
	* @throws UnsupportedOperationException
	*/
	SecretKey generateKey(AlgorithmParameterSpec* keyParams) override {
		throw UnsupportedOperationException("To generate a key for this encryption object use the generateKey(int keySize) function");
	}

	/**
	* Generates a secret key to initialize the underlying PRP object.
	* @param keySize is the required secret key size in bits.
	* @return the generated secret key.
	*/
	SecretKey generateKey(int keySize) override;

	/**
	* This function encrypts a plaintext. It lets the system choose the random IV.
	* @param plaintext should be an instance of ByteArrayPlaintext.
	* @return  an IVCiphertext, which contains the IV used and the encrypted data.
	* @throws IllegalStateException if no secret key was set.
	* @throws IllegalArgumentException if the given plaintext is not an instance of ByteArrayPlaintext.
	*/
	shared_ptr<SymmetricCiphertext> encrypt(Plaintext* plaintext) override;

	/**
	* This function encrypts a plaintext. It lets the user choose the random IV.
	* @param plaintext should be an instance of ByteArrayPlaintext.
	* @param iv random bytes to use in the encryption of the message.
	* @return an IVCiphertext, which contains the IV used and the encrypted data.
	*/
	shared_ptr<SymmetricCiphertext> encrypt(Plaintext* plaintext, vector<byte> iv) override;

	/**
	* Decrypts the given ciphertext using the underlying prp as the block cipher function.
	*
	* @param ciphertext the given ciphertext to decrypt. MUST be an instance of IVCiphertext.
	* @return the plaintext object containing the decrypted ciphertext.
	*/
	shared_ptr<Plaintext> decrypt(SymmetricCiphertext* ciphertext) override;
};

/**
* This class performs the randomized Counter (CTR) Mode encryption and decryption, using OpenSSL library.
* By definition, this encryption scheme is CPA-secure.
*
* @author Cryptography and Computer Security Research Group Department of Computer Science Bar-Ilan University (Moriya Farbstein)
*
*/
class OpenSSLCTREncRandomIV : public OpenSSLEncWithIVAbs, public CTREnc {

	//Native function that sets the encryption and decryption objects with the underlying prpName and key.
	//private native void setKey(long enc, long dec, String prpName, byte[] secretKey);
protected:


	/**
	* Checks the validity of the given prp name.
	* In the CBC case, the valid prp name is AES.
	*/
	bool checkExistance(string prpName) override {
		//If the given name is "AES" return true; otherwise, return false.
		if (prpName == "AES") return true;
		return false;
	}

public:
	/**
	* Gets the name of the underlying prp that determines the type of encryption that will be performed.
	* A default source of randomness is used.
	* @param prp the underlying pseudorandom permutation to get the name of.
	*/
	OpenSSLCTREncRandomIV(PseudorandomPermutation* prp) : OpenSSLEncWithIVAbs(prp) { doConstruct(); }

	/**
	* Sets the name of a Pseudorandom permutation and the name of a Random Number Generator Algorithm to use to generate the source of randomness.<p>
	* @param prpName the name of a specific Pseudorandom permutation, for example "AES".
	* @param randNumGenAlg  the name of the RNG algorithm, for example "SHA1PRNG".
	* @throws NoSuchAlgorithmException  if the given randNumGenAlg is not a valid random number generator.
	*/
	OpenSSLCTREncRandomIV(string prpName) : OpenSSLEncWithIVAbs(prpName) { doConstruct(); }

	/**
	* Supply the encryption scheme with a Secret Key.
	*/
	void setKey(SecretKey secretKey) {
		OpenSSLEncWithIVAbs::setKey(secretKey);
		
		//Create the requested block cipher according to the given prpName.
		const EVP_CIPHER* cipher;
		//In case the given prp name is AES, the actual object to use depends on the key size.
		auto key = secretKey.getEncoded();
		int len = key.size() * 8; //number of bit in key.

		switch (len) {
			case 128: cipher = EVP_aes_128_ctr();
							   break;
			case 192: cipher = EVP_aes_192_ctr();
							   break;
			case 256: cipher = EVP_aes_256_ctr();
							   break;
			default: break;
		}
		
		//Initialize the encryption objects with the key.
		EVP_EncryptInit(enc, cipher, (unsigned char*)key.data(), NULL);
		EVP_DecryptInit(dec, cipher, (unsigned char*)key.data(), NULL);
	}

	/**
	* @return the algorithm name - CTR and the underlying prp name.
	*/
	string getAlgorithmName() override { return "CTR Encryption with " + prpName; }
};