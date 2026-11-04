# portus.auth

> Source: `src/portus/auth.pengu`

portus.auth — Basic, Bearer y JWT HS256.

Ninguna función escribe la respuesta: devuelven `none`/`false` y dejan
`WWW-Authenticate` puesto, para que el handler decida el códigos y el cuerpo.

## Ejemplo
```pengu
weave area with context as ref to portus.Context into result of void to portus.HttpError:
    let user as maybe string is calling portus.auth.basic with context, "ada", "secreto"
    if not user.is_present:
        calling context.send_status with 401, "no autorizado"
        return calling portus.ok_void
    calling context.send_text with "hola {user.value}"
    return calling portus.ok_void
```

## Memoria
Las credenciales y los tokens que devuelven estas funciones son copias
propias; libéralas con `banish`.

---

### `Credentials`

> rune Credentials

```pengu
    user as string
    password as string
```

 Usuario y contraseña de Basic.

### `BASIC_REALM`

> **BASIC_REALM** : string

 Cabecera `WWW-Authenticate` de Basic.

### `BEARER_REALM`

> **BEARER_REALM** : string

 Cabecera `WWW-Authenticate` de Bearer.

### `same_secret`

> **same_secret**(a: string, b: string) -> bool

 Compara dos strings en tiempo constante.

### `parse_basic`

> **parse_basic**(context: ref to Context) -> maybe Credentials

 Credenciales Basic de la petición, si la cabecera es válida.

 ## Memoria
 Devuelve credenciales propias; libéralas con `banish` (o deja que el
 llamador lo haga).

### `basic`

> **basic**(context: ref to Context, user: string, password: string) -> maybe string

 Autentica Basic contra `user`/`password`; si falla deja `WWW-Authenticate`.

 ## Memoria
 El string devuelto es propio; libéralo con `banish`.

### `bearer`

> **bearer**(context: ref to Context) -> maybe string

 Token Bearer de la petición, si lo hay.

 ## Memoria
 Devuelve un string propio; libéralo con `banish`.

### `bearer_is`

> **bearer_is**(context: ref to Context, expected: string) -> bool

 Autentica un token Bearer fijo (comparación en tiempo constante).

### `sign_hs256`

> **sign_hs256**(secret: string, claims: string) -> string

 Firma un JWT HS256 con las `claims` que ya son JSON.

 ## Memoria
 Devuelve el token propio; libéralo con `banish`.

### `verify_hs256`

> **verify_hs256**(secret: string, token: string) -> bool

 Comprueba firma y estructura de un JWT HS256.

### `claims`

> **claims**(secret: string, token: string) -> maybe string

 Claims (JSON) de un JWT HS256 si la firma es válida.

 ## Memoria
 Devuelve el JSON propio; libéralo con `banish`.

### `bearer_jwt`

> **bearer_jwt**(context: ref to Context, secret: string) -> maybe string

 JWT del `Authorization: Bearer` con firma válida; deja `WWW-Authenticate`
 si no lo es.

 ## Memoria
 Devuelve las claims propias; libéralas con `banish`.

### `"basic decodifica y valida"`

> test "basic decodifica y valida"

_Undocumented._

### `"jwt firma y verifica"`

> test "jwt firma y verifica"

_Undocumented._

