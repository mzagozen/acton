{-# LANGUAGE MultiParamTypeClasses, FlexibleInstances, FlexibleContexts, DeriveGeneric #-}
module Acton.CodeGen where

import qualified Data.Set
import qualified Acton.Env
import Utils
import Pretty
import Acton.Syntax
import Acton.Names
import Acton.Builtin
import Acton.Printer
import Acton.Prim
import Acton.Env
import Acton.QuickType
import Acton.Subst
import Prelude hiding ((<>))

generate                            :: Acton.Env.Env0 -> Module -> IO (String,String)
generate env m                      = do return (h,c)
  where h                           = render $ hModule env0 m
        c                           = render $ cModule env0 m
        env0                        = genEnv $ setMod (modname m) env


-- Environment --------------------------------------------------------------------------------------

genEnv env0                         = setX env0 GenX{ globalX = [], localX = [], retX = tNone }

type GenEnv                         = EnvF GenX

data GenX                           = GenX { globalX :: [Name], localX :: [Name], retX :: Type }

gdefine te env                      = modX env1 $ \x -> x{ globalX = dom te ++ globalX x }
  where env1                        = define te env

ldefine te env                      = modX env1 $ \x -> x{ localX = dom te ++ localX x }
  where env1                        = define te env

setRet t env                        = modX env $ \x -> x{ retX = t }

global env                          = globalX (envX env) \\ localX (envX env)

defined env                         = globalX (envX env) ++ localX (envX env)

ret env                             = retX $ envX env


-- Helpers ------------------------------------------------------------------------------------------

include env m                       = text "#include" <+> doubleQuotes (gen env m <> text ".h")

modNames (Import _ ms : is)         = [ m | ModuleItem m _ <- ms ] ++ modNames is
modNames (FromImport _ (ModRef (0,Just m)) _ : is)
                                    = m : modNames is
modNames (FromImportAll _ (ModRef (0,Just m)) : is)
                                    = m : modNames is
modNames []                         = []


conParamNames                       = map (Internal CodeGenPass "par") [1..]

-- Header -------------------------------------------------------------------------------------------

hModule env (Module m imps stmts)   = text "#ifndef" <+> gen env m $+$
                                      text "#define" <+> gen env m $+$
                                      include env (name "builtin") $+$
                                      include env (name "rts") $+$
                                      vcat (map (include env) $ modNames imps) $+$
                                      hSuite env stmts $+$
                                      text "void" <+> genTopName env initKW <+> parens empty <> semi $+$
                                      text "#endif"

hSuite env []                       = empty
hSuite env (s:ss)                   = hStmt env s $+$ hSuite (gdefine (envOf s) env) ss

hStmt env (Decl _ ds)               = vmap (stub env1) ds $+$
                                      vmap (typedef env1) ds $+$
                                      vmap (decl env1) ds $+$
                                      vmap (methstub env1) ds
  where env1                        = gdefine (envOf ds) env
hStmt env s                         = vcat [ text "extern" <+> gen env t <+> genTopName env n <> semi | (n,NVar t) <- envOf s]

stub env (Class _ n q a b)          = text "struct" <+> genTopName env n <> semi
stub env Def{}                      = empty

typedef env (Class _ n q a b)       = text "typedef" <+> text "struct" <+> genTopName env n <+> char '*' <> genTopName env n <> semi
typedef env Def{}                   = empty

decl env (Class _ n q a b)          = (text "struct" <+> classname env n <+> char '{') $+$ 
                                      nest 4 (vcat $ stdprefix env ++ initdef : serialize env tc : deserialize env tc : meths) $+$
                                      char '}' <> semi $+$
                                      (text "struct" <+> genTopName env n <+> char '{') $+$ 
                                      nest 4 (classlink env n $+$ properties env tc) $+$ 
                                      char '}' <> semi
  where tc                          = TC (NoQ n) [ tVar v | Quant v _ <- q ]
        initdef : meths             = fields env tc
decl env (Def _ n q p _ a b _ fx)   = gen env (fromJust a) <+> genTopName env n <+> parens (params env $ prowOf p) <> semi

methstub env (Class _ n q a b)      = text "extern" <+> text "struct" <+> classname env n <+> methodtable env n <> semi $+$
                                      gen env t <+> newcon env n <> parens (params env r) <> semi
  where TFun _ _ r _ t              = typeInstOf env (map tVar $ tybound q) (eVar n)
methstub env Def{}                  = empty

fields env c                        = map field te
  where te                          = fullAttrEnv env c
        field (n, NDef sc Static)   = funsig env n (sctype sc) <> semi
        field (n, NDef sc NoDec)    = methsig env c n (sctype sc) <> semi
        field (n, NVar t)           = varsig env n t <> semi
        field (n, NSig sc Static)   = funsig env n (sctype sc) <> semi
        field (n, NSig sc NoDec)    = methsig env c n (sctype sc) <> semi
        field (n, NSig sc Property) = empty

funsig env n (TFun _ _ r _ t)       = gen env t <+> parens (char '*' <> gen env n) <+> parens (params env r)

methsig env c n (TFun _ _ r _ t)    = gen env t <+> parens (char '*' <> gen env n) <+> parens (params env $ posRow (tCon c) r)

params env (TNil _ _)               = empty
params env (TRow _ _ _ t r@TRow{})  = gen env t <> comma <+> params env r
params env (TRow _ _ _ t TNil{})    = gen env t
params env (TRow _ _ _ t TVar{})    = gen env t                                         -- Ignore param tails for now...
params env t                        = error ("codegen unexpected row: " ++ prstr t)

varsig env n t                      = gen env t <+> gen env n

properties env c                    = vmap prop te
  where te                          = fullAttrEnv env c
        prop (n, NSig sc Property)  = varsig env n (sctype sc) <> semi
        prop _                      = empty

stdprefix env                       = [gcinfo env, classid env, superlink env]

gcinfo env                          = text "char" <+> text "*" <> gen env gcinfoKW <> semi

classid env                         = gen env tInt <+> gen env classidKW <> semi

superlink env                       = gen env tSuperclass <+> gen env superclassKW <> semi
  where tSuperclass                 = tCon $ TC qnSuperClass []

qnSuperClass                        = GName mPrim (Derived (name "Super") (name "class"))

serialize env c                     = methsig env c (name "__serialize__") (TFun l0 fxPure serialstate kwdNil tNone) <> semi

deserialize env c                   = funsig env (name "__deserialize__") (TFun l0 fxPure serialstate kwdNil (tCon c)) <> semi

serialstate                         = posRow tSerialstate posNil
  where tSerialstate                = tCon $ TC (GName mPrim (Derived (name "Serial") (name "state"))) []

classlink env n                     = text "struct" <+> classname env n <+> text "*" <> gen env classKW <> semi

classname env n                     = genTopName env (Derived n $ name "class")

methodtable env n                   = genTopName env (tableName n)

methodtable' env (NoQ n)            = methodtable env n
methodtable' env (GName m n)        = gen env $ GName m (tableName n)

newcon env n                        = genTopName env (conName n)

newcon' env (NoQ n)                 = newcon env n
newcon' env (GName m n)             = gen env $ GName m (conName n)

tableName n                         = Derived n $ name "methods"
conName n                           = Derived n $ name "new"

classKW                             = primKW "class"
gcinfoKW                            = primKW "GCINFO"
classidKW                           = primKW "class_id"
superclassKW                        = primKW "superclass"
componentsKW                        = name "components"

primTuple                           = gPrim "tuple"
primNoneType                        = gPrim "NoneType"
primNone                            = gPrim "None"
primTrue                            = gPrim "True"
primFalse                           = gPrim "False"

primAPP                             = gPrim "APP"
primNEW                             = gPrim "NEW"
primNEWCC                           = gPrim "NEWCC"
primRegister                        = gPrim "register"

primToInt                           = name "to$int"
primToFloat                         = name "to$float"
primToStr                           = name "to$str"
primToBytearray                     = name "to$bytearray"

tmpV                                = primKW "tmp"


-- Implementation -----------------------------------------------------------------------------------

cModule env (Module m imps stmts)   = include env m $+$
                                      declModule env stmts $+$
                                      text "int" <+> genTopName env initFlag <+> equals <+> text "0" <> semi $+$
                                      (text "void" <+> genTopName env initKW <+> parens empty <+> char '{') $+$
                                      nest 4 (text "if" <+> parens (genTopName env initFlag) <+> text "return" <> semi $+$
                                              genTopName env initFlag <+> equals <+> text "1" <> semi $+$
                                              initImports $+$
                                              initModule env stmts) $+$
                                      char '}'
  where initImports                 = vcat [ gen env (GName m initKW) <> parens empty <> semi | m <- modNames imps ]


declModule env []                   = empty
declModule env (Decl _ ds : ss)     = vcat [ declDecl env1 d | d <- ds ] $+$
                                      declModule env1 ss
  where env1                        = gdefine (envOf ds) env
        te                          = envOf ds
declModule env (s : ss)             = vcat [ gen env t <+> genTopName env n <> semi | (n,NVar t) <- te ] $+$
                                      declModule env1 ss
  where te                          = envOf s `exclude` defined env
        env1                        = gdefine te env

declDecl env (Def _ n q p KwdNIL (Just t) b d m)
                                    = (gen env t <+> genTopName env n <+> parens (gen env p) <+> char '{') $+$
                                      nest 4 (genSuite env1 b $+$ ret) $+$
                                      char '}'
  where env1                        = setRet t $ ldefine (envOf p) $ defineTVars q env
        ret | fallsthru b           = text "return" <+> gen env primNone <> semi
            | otherwise             = empty
declDecl env (Class _ n q as b)     = vcat [ declDecl env1 d{ dname = methodname n (dname d) } | Decl _ ds <- b', d@Def{} <- ds ] $+$
                                      declCon env1 n q $+$
                                      text "struct" <+> classname env n <+> methodtable env n <> semi
  where b'                          = subst [(tvSelf, tCon $ TC (NoQ n) (map tVar $ tybound q))] b
        env1                        = defineTVars q env



initModule env []                   = empty
initModule env (Decl _ ds : ss)     = vcat [ char '{' $+$ nest 4 (initClassBase env1 n as $+$ initClass env n b) $+$ char '}' | Class _ n q as b <- ds ] $+$
                                      initModule env1 ss
  where env1                        = gdefine (envOf ds) env
initModule env (Signature{} : ss)   = initModule env ss
initModule env (s : ss)             = genStmt env s $+$
                                      vcat [ genTopName env n <+> equals <+> gen env n <> semi | (n,_) <- te ] $+$
                                      initModule env1 ss
  where te                          = envOf s `exclude` defined env
        env1                        = gdefine te env


initClassBase env c as              = methodtable env c <> dot <> gen env gcinfoKW <+> equals <+> doubleQuotes (genTopName env c) <> semi $+$
                                      methodtable env c <> dot <> gen env superclassKW <+> equals <+> super <> semi $+$
                                      vcat [ inherit c' n i | (c',te) <- inheritedAttrs env (NoQ c), (n,i) <- te ]
  where super                       = if null as then text "NULL" else parens (gen env qnSuperClass) <> text "&" <> methodtable' env (tcname $ head as)
        selfsubst                   = subst [(tvSelf, tCon $ TC (NoQ c) [])]
        inherit c' n i              = methodtable env c <> dot <> gen env n <+> equals <+> cast i <> methodtable' env c' <> dot <> gen env n <> semi
          where cast (NVar t)       = parens (gen env $ selfsubst t)
                cast (NDef sc dec)  = parens (gen env (selfsubst $ addSelf (sctype sc) (Just dec)))

initClass env c []                  = gen env primRegister <> parens (char '&' <> methodtable env c) <> semi
initClass env c (Decl _ ds : ss)    = vcat [ methodtable env c <> dot <> gen env n <+> equals <+> genTopName env (methodname c n) <> semi | Def{dname=n} <- ds ] $+$
                                      initClass env1 c ss
  where env1                        = gdefine (envOf ds) env
initClass env c (Signature{} : ss)  = initClass env c ss
initClass env c (s : ss)            = genStmt env s $+$
                                      vcat [ genTopName env c <> dot <> gen env n <+> equals <+> gen env n <> semi | (n,_) <- te ] $+$
                                      initClass env1 c ss
  where te                          = envOf s `exclude` defined env
        env1                        = ldefine te env


initFlag                            = name "done$"

methodname c n                      = Derived c n



class Gen a where
    gen                             :: GenEnv -> a -> Doc


instance (Gen a) => Gen (Maybe a) where
    gen env x                       = maybe empty (gen env) x


instance Gen ModName where
    gen env (ModName ns)            = hcat $ punctuate (char '$') $ map (gen env) ns

instance Gen QName where
    gen env (GName m n)
      | m == mPrim                  = char '$' <> text (nstr n)
      | m == mBuiltin               = char '$' <> text (nstr n)
      | otherwise                   = gen env m <> text "$$" <> text (mkCident $ nstr n)
    gen env (NoQ n)                 = gen env n
    gen env (QName m n)             = error ("Unexpected QName in CodeGen: " ++ prstr (QName m n))

instance Gen Name where
    gen env nm                      = text $ unCkeyword $ mkCident $ nstr nm

mkCident str
  | isCident str                    = str
  | otherwise                       = preEscape $ concat $ map esc str
  where isCident s@(c:cs)           = isAlpha c && all isAlphaNum cs
        isAlpha c                   = c `elem` ['a'..'z'] || c `elem` ['A'..'Z'] || c `elem` ['_','$']
        isAlphaNum c                = isAlpha c || c `elem` ['0'..'9']
        esc c | isAlphaNum c        = [c]
              | otherwise           = '_' : show (fromEnum c) ++ "_"

unCkeyword str
  | str `Data.Set.member` rws       = preEscape str
  | otherwise                       = str
  where rws                         = Data.Set.fromDistinctAscList [
                                        "auto",     "break",    "case",     "char",     "continue", "default",
                                        "default",  "do",       "double",   "else",     "enum",     "extern",
                                        "float",    "for",      "goto",     "if",       "int",      "long",
                                        "register", "return",   "short",    "signed",   "sizeof",   "static",
                                        "struct",   "switch",   "typedef",  "union",    "unsigned", "void",
                                        "volatile", "while"
                                      ]

preEscape str                       = "_$" ++ str


genTopName env n                    = gen env (gname env n)

word                                = text "$WORD"

genSuite env []                     = empty
genSuite env (s:ss)                 = genStmt env s $+$ genSuite (ldefine (envOf s) env) ss
  where te                          = envOf s `exclude` defined env
        env1                        = ldefine te env

genStmt env (Decl _ ds)             = empty
genStmt env (Assign _ [PVar _ n (Just t)] e)
  | n `notElem` defined env         = gen env t <+> gen env n <+> equals <+> genExp env t e <> semi
genStmt env s                       = vcat [ gen env t <+> gen env n <> semi | (n,NVar t) <- te ] $+$
                                      gen env s
  where te                          = envOf s `exclude` defined env

instance Gen Stmt where
    gen env (Expr _ e)              = gen env e <> semi
    gen env (Assign _ [p] e)        = gen env p <+> equals <+> genExp env t e <> semi
      where t                       = typeOf env p
    gen env (MutAssign _ tg e)      = gen env tg <+> equals <+> genExp env t e <> semi
      where t                       = typeOf env tg
    gen env (Pass _)                = empty
    gen env (Return _ Nothing)      = text "return" <+> gen env eNone <> semi
    gen env (Return _ (Just e))     = text "return" <+> genExp env (ret env) e <> semi
    gen env (Break _)               = text "break" <> semi
    gen env (Continue _)            = text "continue" <> semi
    gen env (If _ (b:bs) b2)        = genBranch env "if" b $+$ vmap (genBranch env "else if") bs $+$ genElse env b2
    gen env (While _ e b [])        = (text "while" <+> parens (gen env e) <+> char '{') $+$ nest 4 (genSuite env b) $+$ char '}'
    gen env _                       = empty

genBranch env kw (Branch e b)       = (text kw <+> parens (gen env e) <+> char '{') $+$ nest 4 (genSuite env b) $+$ char '}'

genElse env []                      = empty
genElse env b                       = (text "else" <+> char '{') $+$ nest 4 (genSuite env b) $+$ char '}'

instance Gen PosPar where
    gen env (PosPar n t _ PosNIL)   = gen env t <+> gen env n
    gen env (PosPar n t _ p)        = gen env t <+> gen env n <> comma <+> gen env p
    gen env PosNIL                  = empty

instance Gen PosArg where
    gen env (PosArg e PosNil)       = gen env e
    gen env (PosArg e p)            = gen env e <> comma <+> gen env p
    gen env PosNil                  = empty

formatlong s                        = s
  where format []                   = posNil
        format ('%':s)              = flags s
        format (c:s)                = format s
        flags (f:s)
          | f `elem` "#0- +"        = flags s
        flags s                     = width s
        width ('*':s)               = posRow tInt (dot s)
        width (n:s)
          | n `elem` "123456789"    = dot (dropWhile (`elem` "0123456789") s)
        width s                     = dot s
        dot ('.':s)                 = prec s
        dot s                       = len s
        prec ('*':s)                = posRow tInt (len s)
        prec (n:s)
          | n `elem` "0123456789"   = len (dropWhile (`elem` "0123456789") s)
        prec s                      = len s
        len (l:s)
          | l `elem` "hlL"          = conv s
        len s                       = conv s
        conv (t:s)
          | t `elem` "diouxXc"      = posRow tInt (format s)
          | t `elem` "eEfFgG"       = posRow tFloat (format s)
          | t `elem` "rsa"          = posRow tStr (format s)
          | t == '%'                = format s

genCall env t0 [] (TApp _ e ts) p   = genCall env t0 ts e p
genCall env t0 [_,t] (Var _ n) (PosArg e PosNil)
  | n == primCAST                   = parens (gen env t) <> gen env e
genCall env t0 [row] (Var _ n) p
  | qn == qnPrint                   = gen env qn <> parens (pretty i <> if i > 0 then comma <+> gen env p else empty)
  where i                           = nargs p
        qn                          = unalias env n
genCall env t0 [row] (Var _ n) (PosArg s@Strings{} (PosArg tup PosNil))
  | n == primFORMAT                 = gen env n <> parens (genStr env s <> unbox row (flatten tup))
  where unbox (TNil _ _) p          = empty
        unbox (TRow _  _ _ t r) (PosArg e p)
          | t == tStr               = comma <+> expr <> text "->str" <> unbox r p
          | otherwise               = comma <+> expr <> text "->val" <> unbox r p
          where expr                = parens (parens (gen env t) <> gen env e)
        flatten (Tuple _ p KwdNil)  = p
        flatten e                   = foldr PosArg PosNil $ map (DotI l0 e) [0..]
genCall env t0 ts e@(Var _ n) p
  | NClass{} <- info                = genNew env ts n p
  | NDef{} <- info                  = gen env e <> parens (gen env p)
  where info                        = findQName n env
genCall env t0 ts e0@(Dot _ e n) p  = genDotCall env (snd $ schemaOf env e0) e n p
genCall env t0 ts e p               = genEnter env ts e callKW p


genNew env ts n p                   = newcon' env n <> parens (gen env p)

declCon env n q                     = (gen env tRes <+> newcon env n <> parens (gen env pars) <+> char '{') $+$
                                      nest 4 (gen env tObj <+> gen env tmpV <+> equals <+> malloc env (gname env n) <> semi $+$
                                              gen env tmpV <> text "->" <> gen env1 classKW <+> equals <+> char '&' <> methodtable env1 n <> semi $+$
                                              initcall env1) $+$
                                      char '}'
  where TFun _ fx r _ t             = typeInstOf env (map tVar $ tybound q) (eVar n)
        tObj                        = tCon $ TC (unalias env $ NoQ n) (map tVar $ tybound q)
        tRes                        = if t == tR then tR else tObj
        pars                        = pPar conParamNames r
        args                        = pArg pars
        initcall env | t == tR      = text "return" <+> methodtable env n <> dot <> gen env initKW <> parens (gen env tmpV <> comma <+> gen env (retobj args)) <> semi
                     | otherwise    = methodtable env n <> dot <> gen env initKW <> parens (gen env tmpV <> comma' (gen env args)) <> semi $+$
                                      text "return" <+> gen env tmpV <> semi
        retobj (PosArg e PosNil)    = PosArg (eCall (tApp (eQVar primCONSTCONT) [fx,t]) [eVar tmpV, e]) PosNil
        retobj (PosArg e p)         = PosArg e (retobj p)
        env1                        = ldefine ((tmpV, NVar tObj) : envOf pars) env

malloc env n                        = text "malloc" <> parens (text "sizeof" <> parens (text "struct" <+> gen env n))

comma' x                            = if isEmpty x then empty else comma <+> x

genDotCall env dec e@(Var _ x) n p
  | NClass{} <- info, Just _ <- dec = methodtable' env x <> text "." <> gen env n <> parens (gen env p)
  | NClass{} <- info                = genEnter env [] (eDot e n) callKW p
  where info                        = findQName x env
genDotCall env dec e n p
  | Just NoDec <- dec               = genEnter env [] e n p
  | Just Static <- dec              = gen env e <> text "->" <> gen env classKW <> text "->" <> gen env n <> parens (gen env p)
genDotCall env dec e n p            = genEnter env [] (eDot e n) callKW p


genDot env ts e@(Var _ x) n
  | NClass{} <- findQName x env     = methodtable' env x <> text "." <> gen env n
genDot env [] e n                   = gen env e <> text "->" <> gen env n
genDot env ts e n                   = gen env e <> text "->" <> gen env n


genEnter env ts e n p
  | costly e                        = parens (lbrace <+> (gen env t <+> gen env tmpV <+> equals <+> gen env e <> semi $+$
                                                          genEnter env1 [] (eVar tmpV) n p <> semi) <+> rbrace)
  where costly Var{}                = False
        costly (Dot _ e n)          = costly e
        costly (DotI _ e i)         = costly e
        costly e                    = True
        t                           = typeInstOf env ts e
        env1                        = ldefine [(tmpV,NVar t)] env
genEnter env ts e n PosNil          = gen env e <> text "->" <> gen env classKW <> text "->" <> gen env n <> parens (gen env e)
genEnter env ts e n p               = gen env e <> text "->" <> gen env classKW <> text "->" <> gen env n <> parens (gen env (PosArg e p))

genInst env ts e@Var{}              = gen env e
genInst env ts (Dot _ e n)          = genDot env ts e n

adjustC TVar{} TVar{} e             = e
adjustC TNone{} t' e                = e
adjustC (TCon _ c) (TCon _ c') e
  | tcname c == tcname c'           = e
adjustC (TOpt _ t) t' e             = adjustC t t' e
adjustC t (TOpt _ t') e             = adjustC t t' e
adjustC t t' e                      = adjust t t' e

genExp env t' e                     = gen env (adjustC t t' e')
  where (t, e')                     = typeOf' env e

instance Gen Expr where
    gen env (Var _ (NoQ n))
      | n `elem` global env         = genTopName env n
      | isAlias n env               = genTopName env n
    gen env (Var _ n)
      | NClass{} <- findQName n env = newcon' env n
      | otherwise                   = gen env n
    gen env (Int _ _ str)           = gen env primToInt <> parens (text str)
    gen env (Float _ _ str)         = gen env primToFloat <> parens (text str)
    gen env (Bool _ True)           = gen env primTrue
    gen env (Bool _ False)          = gen env primFalse
    gen env (None _)                = gen env primNone
    gen env e@Strings{}             = gen env primToStr <> parens (genStr env e)
    gen env e@BStrings{}            = gen env primToBytearray <> parens (genStr env e)
    gen env e0@(Call _ e p KwdNil)  = genCall env (typeOf env e0) [] e p
    gen env (TApp _ e ts)           = genInst env ts e
    gen env (IsInstance _ e c)      = gen env primISINSTANCE <> parens (gen env e <> comma <+> gen env (globalize env c))
    gen env (Dot _ e n)             = genDot env [] e n
    gen env (DotI _ e i)            = gen env e <> text "->" <> gen env componentsKW <> brackets (pretty i)
    gen env (RestI _ e i)           = text "CodeGen for tuple tail not implemented"
    gen env (Tuple _ p KwdNil)      = parens (lbrace <+> (
                                        gen env n <+> tmp <+> equals <+> malloc env n <> semi $+$
                                        tmp <> text "->" <> gen env classKW <+> equals <+> char '&' <> table <> semi $+$
                                        table <> dot <> gen env initKW <> parens (tmp <> comma <+> text (show $ nargs p) <> comma' (gen env p)) <> semi $+$
                                        tmp <> semi) <+> rbrace)
      where n                       = primTuple
            table                   = methodtable' env n
            tmp                     = gen env tmpV
    gen env (List _ es)             = parens (lbrace <+> (
                                        gen env n <+> tmp <+> equals <+> newcon' env n <> parens (text "NULL" <> comma <+> text "NULL") <> semi $+$
                                        vcat [ append <> parens (pars e) <> semi | e <- es ] $+$
                                        tmp <> semi) <+> rbrace)
      where n                       = qnList
            tmp                     = gen env tmpV
            w                       = gen env witSequenceList
            append                  = w <> text "->" <> gen env classKW <> text "->" <> gen env appendKW
            pars e                  = w <> comma <+> tmp <> comma <+> gen env e
        -- brackets (commaSep (gen env) es)
    gen env e@BinOp{}               = genPrec env 0 e
    gen env e@UnOp{}                = genPrec env 0 e
    gen env e@Cond{}                = genPrec env 0 e

genStr env s                        = doubleQuotes $ text $ tail $ init $ concat $ sval s

nargs                               :: PosArg -> Int
nargs PosNil                        = 0
nargs (PosArg _ p)                  = 1 + nargs p

{-
We assign precedences and associativity to remaining operators as follows

   Not   4  ---
   And   3  left
   Or    2  left
   ?:    1  right

Note that the expression between ? and : in the ternary conditional operator is parsed as if it was parenthesized, 
so we never print parentheses around it. The remaining binary operator _ ?: _ has lower precedence than the other 
boolean operators and associates to the right.

We never need to put unary negated expressions in parentheses, since all higher precedence operators have been 
eliminated in previous passes.
-}

genPrec env _ (UnOp _ Not e)            = text "!" <> genPrec env 4 e
genPrec env n e@(BinOp _ e1 And e2)     = parensIf (n > 3) (genPrec env 3 e1 <+> text "&&" <+> genPrec env 4 e2)
genPrec env n e@(BinOp _ e1 Or e2)      = parensIf (n > 2) (genPrec env 2 e1 <+> text "||" <+> genPrec env 3 e2)
genPrec env n (Cond _ e1 e e2)          = parensIf (n > 1) (genPrec env 2 e <+> text "?" <+> gen env e1 <+> text ":" <+> genPrec env 1 e2)
genPrec env _ e                         = gen env e

instance Gen Elem where
    gen env (Elem e)                = gen env e

instance Gen Pattern where
    gen env (PVar _ n _)            = gen env n

instance Gen TSchema where
    gen env (TSchema _ _ t)         = gen env t

instance Gen TVar where
    gen env (TV k n)                = word

instance Gen TCon where
    gen env (TC n ts)               = gen env (globalize env n)
    
instance Gen Type where
    gen env (TVar _ v)              = gen env v
    gen env (TCon  _ c)             = gen env c
    gen env (TFun _ _ p _ t)        = gen env t <+> parens (char '*') <+> parens (gen env p)
    gen env (TTuple _ pos _)        = gen env primTuple
    gen env (TUnion _ as)           = word
    gen env (TOpt _ t)              = gen env t
    gen env (TNone _)               = gen env primNoneType
    gen env (TWild _)               = word
    gen env (TRow _ _ _ t TNil{})   = gen env t
    gen env (TRow _ _ _ t r)        = gen env t <> comma <+> gen env r
    gen env (TNil _ _)              = empty

