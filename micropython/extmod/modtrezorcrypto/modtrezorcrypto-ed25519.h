/*
 * Copyright (c) Pavol Rusnak, SatoshiLabs
 *
 * Licensed under TREZOR License
 * see LICENSE file for details
 */

#include "py/objstr.h"

#include "trezor-crypto/ed25519-donna/ed25519.h"

#include "rand.h"

typedef struct _mp_obj_Ed25519_t {
    mp_obj_base_t base;
} mp_obj_Ed25519_t;

STATIC mp_obj_t mod_TrezorCrypto_Ed25519_make_new(const mp_obj_type_t *type, size_t n_args, size_t n_kw, const mp_obj_t *args) {
    mp_arg_check_num(n_args, n_kw, 0, 0, false);
    mp_obj_Ed25519_t *o = m_new_obj(mp_obj_Ed25519_t);
    o->base.type = type;
    return MP_OBJ_FROM_PTR(o);
}

/// def trezor.crypto.curve.ed25519.generate_secret() -> bytes:
///     '''
///     Generate secret key.
///     '''
STATIC mp_obj_t mod_TrezorCrypto_Ed25519_generate_secret(mp_obj_t self) {
    vstr_t vstr;
    vstr_init_len(&vstr, 32);
    random_buffer((uint8_t *)vstr.buf, 32);
    // taken from https://cr.yp.to/ecdh.html
    vstr.buf[0] &= 248;
    vstr.buf[31] &= 127;
    vstr.buf[31] |= 64;
    return mp_obj_new_str_from_vstr(&mp_type_bytes, &vstr);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_1(mod_TrezorCrypto_Ed25519_generate_secret_obj, mod_TrezorCrypto_Ed25519_generate_secret);

/// def trezor.crypto.curve.ed25519.publickey(secret_key: bytes) -> bytes:
///     '''
///     Computes public key from secret key.
///     '''
STATIC mp_obj_t mod_TrezorCrypto_Ed25519_publickey(mp_obj_t self, mp_obj_t secret_key) {
    mp_buffer_info_t sk;
    mp_get_buffer_raise(secret_key, &sk, MP_BUFFER_READ);
    if (sk.len != 32) {
        mp_raise_ValueError("Invalid length of secret key");
    }
    vstr_t vstr;
    vstr_init_len(&vstr, 32);
    ed25519_publickey(*(const ed25519_secret_key *)sk.buf, *(ed25519_public_key *)vstr.buf);
    return mp_obj_new_str_from_vstr(&mp_type_bytes, &vstr);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(mod_TrezorCrypto_Ed25519_publickey_obj, mod_TrezorCrypto_Ed25519_publickey);

/// def trezor.crypto.curve.ed25519.sign(secret_key: bytes, message: bytes) -> bytes:
///     '''
///     Uses secret key to produce the signature of message.
///     '''
STATIC mp_obj_t mod_TrezorCrypto_Ed25519_sign(mp_obj_t self, mp_obj_t secret_key, mp_obj_t message) {
    mp_buffer_info_t sk, msg;
    mp_get_buffer_raise(secret_key, &sk, MP_BUFFER_READ);
    mp_get_buffer_raise(message, &msg, MP_BUFFER_READ);
    if (sk.len != 32) {
        mp_raise_ValueError("Invalid length of secret key");
    }
    if (msg.len == 0) {
        mp_raise_ValueError("Empty data to sign");
    }
    ed25519_public_key pk;
    ed25519_publickey(*(const ed25519_secret_key *)sk.buf, pk);
    vstr_t vstr;
    vstr_init_len(&vstr, 64);
    ed25519_sign(msg.buf, msg.len, *(const ed25519_secret_key *)sk.buf, pk, *(ed25519_signature *)vstr.buf);
    return mp_obj_new_str_from_vstr(&mp_type_bytes, &vstr);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_3(mod_TrezorCrypto_Ed25519_sign_obj, mod_TrezorCrypto_Ed25519_sign);

/// def trezor.crypto.curve.ed25519.verify(public_key: bytes, signature: bytes, message: bytes) -> bool:
///     '''
///     Uses public key to verify the signature of the message.
///     Returns True on success.
///     '''
STATIC mp_obj_t mod_TrezorCrypto_Ed25519_verify(size_t n_args, const mp_obj_t *args) {
    mp_buffer_info_t pk, sig, msg;
    mp_get_buffer_raise(args[1], &pk, MP_BUFFER_READ);
    mp_get_buffer_raise(args[2], &sig, MP_BUFFER_READ);
    mp_get_buffer_raise(args[3], &msg, MP_BUFFER_READ);
    if (pk.len != 32) {
        mp_raise_ValueError("Invalid length of public key");
    }
    if (sig.len != 64) {
        mp_raise_ValueError("Invalid length of signature");
    }
    if (msg.len == 0) {
        mp_raise_ValueError("Empty data to verify");
    }
    return (0 == ed25519_sign_open(msg.buf, msg.len, *(const ed25519_public_key *)pk.buf, *(const ed25519_signature *)sig.buf)) ? mp_const_true : mp_const_false;
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(mod_TrezorCrypto_Ed25519_verify_obj, 4, 4, mod_TrezorCrypto_Ed25519_verify);

/// def trezor.crypto.curve.ed25519.cosi_combine_publickeys(public_keys: list) -> bytes:
///     '''
///     Combines a list of public keys used in COSI cosigning scheme
///     '''
STATIC mp_obj_t mod_TrezorCrypto_Ed25519_cosi_combine_publickeys(mp_obj_t self, mp_obj_t public_keys) {
    size_t pklen;
    mp_obj_t *pkitems;
    mp_obj_get_array(public_keys, &pklen, &pkitems);
    if (pklen > 15) {
        mp_raise_ValueError("Can't combine more than 15 public keys");
    }
    mp_buffer_info_t buf;
    ed25519_public_key pks[pklen];
    for (int i = 0; i < pklen; i++) {
        mp_get_buffer_raise(pkitems[i], &buf, MP_BUFFER_READ);
        if (buf.len != 32) {
            mp_raise_ValueError("Invalid length of public key");
        }
        memcpy(pks[i], buf.buf, buf.len);
    }
    vstr_t vstr;
    vstr_init_len(&vstr, 32);
    if (0 != ed25519_cosi_combine_publickeys(*(ed25519_public_key *)vstr.buf, pks, pklen)) {
        mp_raise_ValueError("Error combining public keys");
    }
    return mp_obj_new_str_from_vstr(&mp_type_bytes, &vstr);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_2(mod_TrezorCrypto_Ed25519_cosi_combine_publickeys_obj, mod_TrezorCrypto_Ed25519_cosi_combine_publickeys);

/// def trezor.crypto.curve.ed25519.cosi_combine_signatures(R: bytes, signatures: list) -> bytes:
///     '''
///     Combines a list of signatures used in COSI cosigning scheme
///     '''
STATIC mp_obj_t mod_TrezorCrypto_Ed25519_cosi_combine_signatures(mp_obj_t self, mp_obj_t R, mp_obj_t signatures) {
    mp_buffer_info_t sigR;
    mp_get_buffer_raise(R, &sigR, MP_BUFFER_READ);
    if (sigR.len != 32) {
        mp_raise_ValueError("Invalid length of R");
    }
    size_t siglen;
    mp_obj_t *sigitems;
    mp_obj_get_array(signatures, &siglen, &sigitems);
    if (siglen > 15) {
        mp_raise_ValueError("Can't combine more than 15 COSI signatures");
    }
    mp_buffer_info_t buf;
    ed25519_cosi_signature sigs[siglen];
    for (int i = 0; i < siglen; i++) {
        mp_get_buffer_raise(sigitems[i], &buf, MP_BUFFER_READ);
        if (buf.len != 32) {
            mp_raise_ValueError("Invalid length of COSI signature");
        }
        memcpy(sigs[i], buf.buf, buf.len);
    }
    vstr_t vstr;
    vstr_init_len(&vstr, 64);
    ed25519_cosi_combine_signatures(*(ed25519_signature *)vstr.buf, *(const ed25519_public_key *)sigR.buf, sigs, siglen);
    return mp_obj_new_str_from_vstr(&mp_type_bytes, &vstr);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_3(mod_TrezorCrypto_Ed25519_cosi_combine_signatures_obj, mod_TrezorCrypto_Ed25519_cosi_combine_signatures);

/// def trezor.crypto.curve.ed25519.cosi_sign(secret_key: bytes, message: bytes, nonce: bytes, sigR: bytes, combined_pubkey: bytes) -> bytes:
///     '''
///     Produce signature of message using COSI cosigning scheme
///     '''
STATIC mp_obj_t mod_TrezorCrypto_Ed25519_cosi_sign(size_t n_args, const mp_obj_t *args) {
    mp_buffer_info_t sk, msg, nonce, sigR, pk;
    mp_get_buffer_raise(args[1], &sk, MP_BUFFER_READ);
    mp_get_buffer_raise(args[2], &msg, MP_BUFFER_READ);
    mp_get_buffer_raise(args[3], &nonce, MP_BUFFER_READ);
    mp_get_buffer_raise(args[4], &sigR, MP_BUFFER_READ);
    mp_get_buffer_raise(args[5], &pk, MP_BUFFER_READ);
    if (sk.len != 32) {
        mp_raise_ValueError("Invalid length of secret key");
    }
    if (nonce.len != 32) {
        mp_raise_ValueError("Invalid length of nonce");
    }
    if (sigR.len != 32) {
        mp_raise_ValueError("Invalid length of R");
    }
    if (pk.len != 32) {
        mp_raise_ValueError("Invalid length of aggregated public key");
    }
    vstr_t vstr;
    vstr_init_len(&vstr, 32);
    ed25519_cosi_sign(msg.buf, msg.len, *(const ed25519_secret_key *)sk.buf, *(const ed25519_secret_key *)nonce.buf, *(const ed25519_public_key *)sigR.buf, *(const ed25519_secret_key *)pk.buf, *(ed25519_cosi_signature *)vstr.buf);
    return mp_obj_new_str_from_vstr(&mp_type_bytes, &vstr);
}
STATIC MP_DEFINE_CONST_FUN_OBJ_VAR_BETWEEN(mod_TrezorCrypto_Ed25519_cosi_sign_obj, 6, 6, mod_TrezorCrypto_Ed25519_cosi_sign);

STATIC const mp_rom_map_elem_t mod_TrezorCrypto_Ed25519_locals_dict_table[] = {
    { MP_ROM_QSTR(MP_QSTR_generate_secret), MP_ROM_PTR(&mod_TrezorCrypto_Ed25519_generate_secret_obj) },
    { MP_ROM_QSTR(MP_QSTR_publickey), MP_ROM_PTR(&mod_TrezorCrypto_Ed25519_publickey_obj) },
    { MP_ROM_QSTR(MP_QSTR_sign), MP_ROM_PTR(&mod_TrezorCrypto_Ed25519_sign_obj) },
    { MP_ROM_QSTR(MP_QSTR_verify), MP_ROM_PTR(&mod_TrezorCrypto_Ed25519_verify_obj) },
    { MP_ROM_QSTR(MP_QSTR_cosi_combine_publickeys), MP_ROM_PTR(&mod_TrezorCrypto_Ed25519_cosi_combine_publickeys_obj) },
    { MP_ROM_QSTR(MP_QSTR_cosi_combine_signatures), MP_ROM_PTR(&mod_TrezorCrypto_Ed25519_cosi_combine_signatures_obj) },
    { MP_ROM_QSTR(MP_QSTR_cosi_sign), MP_ROM_PTR(&mod_TrezorCrypto_Ed25519_cosi_sign_obj) },
};
STATIC MP_DEFINE_CONST_DICT(mod_TrezorCrypto_Ed25519_locals_dict, mod_TrezorCrypto_Ed25519_locals_dict_table);

STATIC const mp_obj_type_t mod_TrezorCrypto_Ed25519_type = {
    { &mp_type_type },
    .name = MP_QSTR_Ed25519,
    .make_new = mod_TrezorCrypto_Ed25519_make_new,
    .locals_dict = (void*)&mod_TrezorCrypto_Ed25519_locals_dict,
};
