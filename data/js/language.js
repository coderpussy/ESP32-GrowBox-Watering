// Default language
export let currentLanguage = 'en';

async function loadLanguage(lang) {
    const response = await fetch(`./lang/${lang}.json`);
    const translations = await response.json();
    return translations;
}

export async function setLanguage(lang) {
    currentLanguage = lang;
    const translations = await loadLanguage(lang);
    applyTranslations(translations);
}

function applyTranslations(translations) {
    document.querySelectorAll('[data-translate]').forEach(element => {
        const key = element.getAttribute('data-translate');
        if (translations[key]) {
            // Check if the element is an input or button
            if (element.tagName === 'INPUT' && element.type === 'button') {
                element.value = translations[key]; // Update button value
            } else if (element.tagName === 'INPUT' && element.hasAttribute('placeholder')) {
                element.placeholder = translations[key]; // Update placeholder text
            } else {
                element.innerText = translations[key]; // Update inner text for other elements
            }
        }
    });
}