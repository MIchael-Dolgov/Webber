        const blocks = document.querySelectorAll('.block');
        const texts = ["A", "B", "C"];
        
        setTimeout(() => {
            console.log("Js Loaded");
            blocks.forEach((block, index) => {
                block.textContent = texts[index];
            });
        }, 2000);