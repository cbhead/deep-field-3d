/**
 * Deep Field 3D — realistic deep-space skybox.
 * One full-sphere ShaderMaterial (BackSide, fog-exempt, no depth write) that
 * ray-marches nothing and ray-casts everything: a physically-coloured starfield
 * (two magnitude layers, black-body tints), a Milky Way band with dust lanes,
 * faint nebulosity, a sun disc with corona, and a nearby planet (gas giant or
 * rock/ice world) with soft terminator, Fresnel atmosphere and limb glow, plus
 * an optional moon. Deterministic, no textures, ~4k tris. Names prefixed
 * `sky_` so bounds/stat code ignores it. GLB stand-in for the HDR bake.
 */
const V = `varying vec3 vDir; void main(){ vDir = normalize((modelMatrix * vec4(position,1.)).xyz - cameraPosition); gl_Position = projectionMatrix * modelViewMatrix * vec4(position,1.); }`;
const F = `precision highp float; varying vec3 vDir;
uniform vec3 uSun, uSunCol, uPlanet, uPlanetAxis, uPal0, uPal1, uPal2, uAtmo, uMoon, uMwN, uMwC, uNebCol;
uniform float uPlanetR, uMoonR, uMwStrength, uNebStrength, uExposure; uniform int uKind;
float hash(vec3 p){ p=fract(p*.3183099+.1); p*=17.; return fract(p.x*p.y*p.z*(p.x+p.y+p.z)); }
vec3 hash3(vec3 p){ return vec3(hash(p), hash(p+31.7), hash(p+77.3)); }
float noise(vec3 x){ vec3 i=floor(x), f=fract(x); f=f*f*(3.-2.*f);
  return mix(mix(mix(hash(i),hash(i+vec3(1,0,0)),f.x),mix(hash(i+vec3(0,1,0)),hash(i+vec3(1,1,0)),f.x),f.y),
             mix(mix(hash(i+vec3(0,0,1)),hash(i+vec3(1,0,1)),f.x),mix(hash(i+vec3(0,1,1)),hash(i+vec3(1,1,1)),f.x),f.y),f.z); }
float fbm(vec3 p){ float a=.5,s=0.; for(int i=0;i<6;i++){ s+=a*noise(p); p=p*2.03+vec3(13.1,7.7,3.3); a*=.5; } return s; }
float fbm4(vec3 p){ float a=.5,s=0.; for(int i=0;i<4;i++){ s+=a*noise(p); p=p*2.03+vec3(13.1,7.7,3.3); a*=.5; } return s; }
vec3 bb(float t){ return mix(mix(vec3(1.,.58,.36), vec3(1.,.94,.86), smoothstep(0.,.55,t)), vec3(.68,.78,1.), smoothstep(.55,1.,t)); }
// Stars are sized in screen pixels (fwidth of the view ray), so they stay pin-sharp at any zoom/resolution.
vec3 stars(vec3 d, float S, float density, float bright, float seed, float px){
  vec3 c=floor(d*S+seed); vec3 h=hash3(c); if(h.x>density) return vec3(0.);
  vec3 sd=normalize(c-seed+.2+.6*hash3(c+9.1)); float ang=length(d-sd);
  float m=pow(h.y,5.); float r=min(px*(.6+1.1*m),.22/S); float i=exp(-ang*ang/(r*r))*(.08+9.*m)*bright;
  return i*bb(h.z); }
bool body(vec3 d, vec3 pd, float angR, vec3 ax, int kind, vec3 p0, vec3 p1, vec3 p2, vec3 atmo, float atmoK, out vec3 col){
  float D=600.; vec3 c=pd*D; float R=D*sin(angR); float b=dot(d,c); float disc=b*b-dot(c,c)+R*R; if(disc<0.) return false;
  vec3 pos=d*(b-sqrt(disc)); vec3 n=normalize(pos-c);
  vec3 t1=normalize(cross(ax,abs(ax.y)<.9?vec3(0,1,0):vec3(1,0,0))), t2=cross(ax,t1);
  float lat=dot(n,ax); float lon=atan(dot(n,t2),dot(n,t1)); vec3 alb;
  if(kind==0){ // gas giant: latitudinal bands sheared by turbulence, storm ovals
    float turb=fbm(n*3.+vec3(lat*3.))*1.4+fbm4(n*11.)*.35; float bnd=lat*11.+turb*2.2;
    float s=.5+.5*sin(bnd); float s2=.5+.5*sin(bnd*2.3+1.7);
    alb=mix(p0,p1,s); alb=mix(alb,p2,s2*smoothstep(.2,.8,fbm4(n*5.+vec3(2.)))*.75);
    float storm=smoothstep(.985,.995,1.-length(vec2((lat+.28)*6.,sin((lon-1.1)*.5)*3.))*.5); alb=mix(alb,p2*1.25,storm);
  } else if(kind==1){ // rock / ice world with oceans and cloud decks
    float h=fbm(n*3.6+vec3(4.)); float land=smoothstep(.47,.53,h); float mtn=smoothstep(.56,.68,h);
    alb=mix(p0,p1,land); alb=mix(alb,p1*.55,mtn); float cap=smoothstep(.62,.85,abs(lat)+.22*fbm4(n*6.));
    alb=mix(alb,p2,cap); float cl=smoothstep(.52,.74,fbm(n*5.5+vec3(11.))+.15*fbm4(n*14.)); alb=mix(alb,vec3(.96,.97,1.),cl*.85);
  } else { // airless moon: regolith with mare and craters
    float h=fbm(n*4.); alb=mix(p0,p1,smoothstep(.45,.6,h)); float cr=fbm4(n*22.); alb*= .78+.35*smoothstep(.45,.7,cr);
  }
  float NdL=dot(n,uSun); float diff=smoothstep(-.06,.28,NdL);
  float mu=max(dot(n,-d),0.); float fres=pow(1.-mu,3.2);
  col=alb*(diff*1.15+.012) + atmo*fres*(diff*1.5+.06)*atmoK;
  return true; }
float limbGlow(vec3 d, vec3 pd, float angR, float k){
  float ang=acos(clamp(dot(d,pd),-1.,1.)); float e=ang-angR; if(e<0.) return 0.;
  vec3 limb=normalize(d-pd*dot(d,pd)); float lit=smoothstep(-.7,.7,dot(normalize(limb*sin(angR)+pd*cos(angR)),uSun));
  return exp(-e/(angR*.035))*(.12+.88*lit)*k + exp(-e/(angR*.18))*(.12+.88*lit)*k*.12; }
void main(){
  vec3 d=normalize(vDir); vec3 col;
  if(body(d,uPlanet,uPlanetR,uPlanetAxis,uKind,uPal0,uPal1,uPal2,uAtmo,1.25,col)){}
  else if(uMoonR>0. && body(d,uMoon,uMoonR,normalize(vec3(.3,1.,.2)),2,vec3(.32,.31,.30),vec3(.19,.19,.2),vec3(0.),vec3(0.),0.,col)){}
  else {
    col=vec3(.002,.0028,.005);
    // nebulosity
    float neb=fbm(d*2.2+vec3(5.)); col+=uNebCol*pow(neb,3.)*uNebStrength;
    // Milky Way
    float bz=dot(d,uMwN); float band=exp(-bz*bz/.055); float core=.5+.5*dot(d,uMwC);
    float n=fbm(d*3.5+vec3(3.)); float dust=fbm(d*8.+vec3(7.)+n);
    float mw=band*(.3+1.1*n)*(.35+1.1*core*core); mw*=1.-.9*smoothstep(.5,.7,dust)*band;
    vec3 mwc=mix(vec3(.5,.58,.85),vec3(1.,.9,.74),core*core); col+=mwc*mw*uMwStrength;
    // stars: dense faint layer, sparse bright layer, a few luminaries
    float px=max(length(fwidth(d)),1e-5);
    col+=stars(d,220.,.05,.35,0.,px)+stars(d,110.,.06,.9,3.,px)+stars(d,40.,.08,2.2,7.,px)+stars(d,14.,.12,4.,19.,px);
    col+=stars(d,260.,.12,.2,11.,px)*(band*.9+.1);
    // sun + corona
    float cs=dot(d,uSun); col+=uSunCol*(smoothstep(.99955,.99985,cs)*40.+pow(max(cs,0.),1600.)*6.+pow(max(cs,0.),60.)*.18+pow(max(cs,0.),6.)*.012);
    col+=uAtmo*limbGlow(d,uPlanet,uPlanetR,1.);
  }
  col=vec3(1.)-exp(-col*uExposure); col=pow(col,vec3(1./2.2));
  gl_FragColor=vec4(col,1.); }`;

const PRESETS = {
  foundry: { // rust gas giant low behind the works, warm sun matching the key light at [-40,18,26]
    sun: [-40, 18, 26], sunCol: [1, .9, .78], planet: [.72, .07, -.69], planetR: .62, axis: [.18, .96, .2], kind: 0,
    pal: [[.42, .17, .08], [.78, .43, .19], [.93, .74, .5]], atmo: [.95, .45, .2], moon: [-.55, .35, -.76], moonR: .028,
    mwN: [.2, .55, .81], mwC: [.8, .1, -.6], mw: .32, neb: [.5, .22, .18], nebK: .08, exposure: .9,
  },
  switchyard: { // blue ice world high to the side, cool sun matching the key light at [20,34,-14]
    sun: [20, 34, -14], sunCol: [.9, .94, 1], planet: [-.62, .28, -.73], planetR: .34, axis: [-.25, .93, .27], kind: 1,
    pal: [[.05, .13, .28], [.36, .33, .25], [.85, .9, .95]], atmo: [.35, .6, 1], moon: [.62, .12, -.78], moonR: .045,
    mwN: [-.35, .5, .79], mwC: [-.3, .2, -.93], mw: .4, neb: [.2, .28, .5], nebK: .07, exposure: .85,
  },
};

export function buildSpaceSky(THREE, kit) {
  const P = PRESETS[kit] || PRESETS.foundry, v = (a) => new THREE.Vector3(...a).normalize(), c = (a) => new THREE.Color(...a);
  const mat = new THREE.ShaderMaterial({
    vertexShader: V, fragmentShader: F, side: THREE.BackSide, depthWrite: false, fog: false, toneMapped: false,
    uniforms: {
      uSun: { value: v(P.sun) }, uSunCol: { value: c(P.sunCol) }, uPlanet: { value: v(P.planet) }, uPlanetAxis: { value: v(P.axis) }, uPlanetR: { value: P.planetR }, uKind: { value: P.kind },
      uPal0: { value: c(P.pal[0]) }, uPal1: { value: c(P.pal[1]) }, uPal2: { value: c(P.pal[2]) }, uAtmo: { value: c(P.atmo) },
      uMoon: { value: v(P.moon) }, uMoonR: { value: P.moonR }, uMwN: { value: v(P.mwN) }, uMwC: { value: v(P.mwC) }, uMwStrength: { value: P.mw },
      uNebCol: { value: c(P.neb) }, uNebStrength: { value: P.nebK }, uExposure: { value: P.exposure },
    },
  });
  mat.name = 'sky_space_' + kit;
  const m = new THREE.Mesh(new THREE.SphereGeometry(700, 64, 32), mat); m.name = 'sky_space_' + kit; m.renderOrder = -10; m.frustumCulled = false;
  m.userData.sky = true; m.userData.gizmo = true;
  return m;
}
